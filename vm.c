#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// ページディレクトリの仮想アドレスに対応するPTEのアドレスを返す
// allocが0を返した場合は必要とされているテーブルのページを割り当てる
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde; // ページディレクトリエントリ
  pte_t *pgtab; // ページテーブル

  pde = &pgdir[PDX(va)]; // 仮想アドレスをインデックスにページディレクトリを取得

  // ページディレクトリが存在している場合
  if(*pde & PTE_P){
    // 仮想アドレスのページオフセット(下位10bit)以外を仮想アドレスに変換する
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  
  // ページディレクトリが存在しない場合
  } else {
    // 割り当てしないよう指定されている、もしくは割り当てに失敗した場合は0を返す
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    
    // 仮想アドレスで指定したページを0クリア
    memset(pgtab, 0, PGSIZE);
    // この設定では過度に寛容だが、必要であればページテーブルの権限で制限することも可能
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)]; // ページテーブルのエントリを返す
}

// "va"で開始する仮想アドレスを"pa"で開始する物理アドレスに変換するため、
// PTE(Page Table Entry)を作成する。引数である"va"と"size"はページ境界で
// アラインメントされる可能性がある
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  // ページ境界になるよう値を切り下げる
  a = (char*)PGROUNDDOWN((uint)va); // 開始アドレス
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1); // ページの終端アドレス

  for(;;){
    // PTEを取得
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    
    // 既に存在している場合
    if(*pte & PTE_P)
      panic("remap");
    
    *pte = pa | perm | PTE_P; // PTEに物理アドレス及び権限、存在を示すフラグを設定
    if(a == last) // 指定の範囲に達した場合は終了
      break;
    
    // 次のページ分へ
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// このテーブルはカーネルのマッピングを定義しており
// これは全てのプロセスのページテーブルに存在する。
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O空間
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // カーネルのtext及びrodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // カーネルのdata及びmemory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // その他のデバイス
};

// ページテーブルの一部をセットアップする
pde_t*
setupkvm(void)
{
  pde_t *pgdir; // ページディレクトリエントリのポインタ
  struct kmap *k; // カーネルのマッピング情報

  if((pgdir = (pde_t*)kalloc()) == 0) // 物理ページフレームを取得
    return 0;
  
  memset(pgdir, 0, PGSIZE); // 0で初期化

  if (P2V(PHYSTOP) > (void*)DEVSPACE) // ハイメモリの領域に侵入してしまっている場合
    panic("PHYSTOP too high");
  
  // カーネルのマッピング情報を元に
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir);
      return 0;
    }
  return pgdir;
}

// 単一のページテーブルをカーネル空間で動作するスケジューラのために割り当てる
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz);
      kfree(mem);
      return 0;
    }
  }
  return newsz;
}

// ユーザプロセスのページを元のサイズ(oldsz)から新たなサイズ(newsz)にメモリサイズを小さくするために
// ユーザのページの割り当てを取り消す。元のサイズと新たなサイズはページ境界でアラインメントされている
// 必要はなく、新たなサイズは元のサイズよりも小さくある必要もない。
// 元のサイズは実際にプロセスが使用しているサイズよりも大きく指定することも可能である。
// 新しいプロセスのサイズを返す。
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  // 新たなサイズが元のサイズよりも大きい場合
  if(newsz >= oldsz)
    return oldsz; // ページの割り当てを取り消す必要がないためそのまま元のサイズを返す

  a = PGROUNDUP(newsz); // ページサイズになるようサイズを切り上げ
  for(; a  < oldsz; a += PGSIZE){ // ページサイズよりも小さい間ページサイズ毎に繰り返し処理する
    pte = walkpgdir(pgdir, (char*)a, 0); // 仮想アドレスに対応するPTEを取得
    
    // PTEが存在しない場合
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE; // 仮想アドレスを構築
    
    // PTEに対応するページが存在する
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte); // PTEに対応する物理アドレスを取得
      
      // 取得に失敗
      if(pa == 0)
        panic("kfree");
      
      char *v = P2V(pa); // 物理アドレスを仮想アドレスに変換
      kfree(v); // 仮想アドレスを開放

      *pte = 0; // PTEの値もゼロに
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
// ユーザ空間のページテーブルと全ての物理メモリページフレームを開放する
void
freevm(pde_t *pgdir)
{
  uint i;

  // ページディレクトリが存在しない
  if(pgdir == 0)
    panic("freevm: no pgdir");
  
  deallocuvm(pgdir, KERNBASE, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i, flags;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    flags = PTE_FLAGS(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)P2V(pa), PGSIZE);
    if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
      kfree(mem);
      goto bad;
    }
  }
  return d;

bad:
  freevm(d);
  return 0;
}

//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

