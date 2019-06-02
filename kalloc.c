// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

// 単方向リスト
struct run {
  struct run *next;
};

// 排他制御用の構造体
struct {
  struct spinlock lock; // ロック変数
  int use_lock; // ロックを使用する必要があるのか
  struct run *freelist; // 単方向リスト
} kmem;

// 初期化は二段階で行われる。
// 1) freelist上のentrypgdirによってマッピングされたページを配置するために
// entrypgdirを使用しながらmain()がkinit1()を呼び出す。
// 2) 全てのページテーブルをインストールした後に
// main()が残りの物理ページを用いてkinit()を呼び出す。
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem"); // kmemという名前でspinlock構造体を初期化
  kmem.use_lock = 0; // ロックを使用するかどうか(しない)
  freerange(vstart, vend); // アドレスで指定したメモリの範囲を初期化する
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  kmem.use_lock = 1;
}

// vstartからvendまでのメモリを開放する
void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart); // ページサイズ以上にならないようにアドレス値を丸める
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE) // 指定された終点アドレスまでページサイズ単位で処理を繰り返す
    kfree(p); // アドレスで指定したページを初期化する
}

// vで参照する物理アドレスで指定されたページを解放する。
// 通常はkalloc()の呼び出しにリターンするはずである。
// 例外としてアロケータの起動がある。
void
kfree(char *v)
{
  struct run *r; // 単方向リスト

  // ページサイズ境界でアラインメントされていない || endよりも小さいアドレス || 許容されている物理メモリよりも大きい場合
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // vを始点にページサイズ分を1で初期化する
  memset(v, 1, PGSIZE);

  if(kmem.use_lock) // ロックを使用する必要がある場合
    acquire(&kmem.lock); // ロックを取得するまでスピンロック
  
  // 空きメモリリストを初期化  
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;

  if(kmem.use_lock) // ロックを使用する必要がある場合
    release(&kmem.lock); // ロックを開放
}

// 4KBの物理メモリページフレームを割り当てる。
// カーネルが使用可能なポインタを返す。
// 割当に失敗した場合には0を返す。
char*
kalloc(void)
{
  struct run *r;

  /* ロックを使用する必要がある場合にはロックを取得する */
  if(kmem.use_lock)
    acquire(&kmem.lock);
  
  r = kmem.freelist; // フリーリストを取得
  if(r) // フリーリストが存在する
    kmem.freelist = r->next; // フリーリストに次の要素を設定(先頭を使用するため)
  
  /* ロックを使用する必要がある場合にはロックを開放する */
  if(kmem.use_lock)
    release(&kmem.lock);
  
  return (char*)r; // フリーリストの先頭要素をかえす
}

