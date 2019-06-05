// Multiprocessor support
// Search memory for MP description structures.
// http://developer.intel.com/design/pentium/datashts/24201606.pdf

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mp.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;

static uchar
sum(uchar *addr, int len)
{
  int i, sum;

  sum = 0;
  for(i=0; i<len; i++)
    sum += addr[i];
  return sum;
}

// MP構造体を指定のアドレス範囲から探す
static struct mp*
mpsearch1(uint a, int len)
{
  uchar *e, *p, *addr;

  addr = P2V(a); // 仮想アドレスに変換
  e = addr+len; // 探索範囲の終端を算出

  for(p = addr; p < e; p += sizeof(struct mp)) // 終端まで繰り返す

    // シグネチャが正しく、データのバイト値の合計が0(!?)の場合 = MP構造体を発見
    if(memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
      return (struct mp*)p;
  return 0;
}

// MP Floating Pointer Structureを探す。
// これはスペックによると次の3つの場所のうちのどこかに存在する。
// 1) EBDA(Extended BIOS Data Area)の先頭1KB
// 2) システムベースメモリの末尾1KB
// 3) BIOS ROMになっている0xE0000 ~ 0xFFFFFの間
static struct mp*
mpsearch(void)
{
  uchar *bda;
  uint p;
  struct mp *mp;

  // https://wiki.osdev.org/Memory_Map_(x86)
  bda = (uchar *) P2V(0x400); // BDA(BIOS Data Area)のアドレスを設定
  // BDAからEBDAのアドレスを取得する
  if((p = ((bda[0x0F]<<8)| bda[0x0E]) << 4)){ // EBDAのアドレスを取得

    // EBDAの先頭1KBからMP Floating Pointer Structureを探す
    if((mp = mpsearch1(p, 1024)))
      return mp;
  
  } else {
    // http://caspar.hazymoon.jp/OpenBSD/annex/bios_data_area.html
    p = ((bda[0x14]<<8)|bda[0x13])*1024; // 認識しているメモリの末尾(メモリサイズ(KB)*1024)を算出
    if((mp = mpsearch1(p-1024, 1024))) // 認識しているメモリの末尾1KBを対象に当該構造体を探す
      return mp;
  }

  // 現時点で見つかっていない場合はBIOS ROMが展開されている範囲を探索
  // http://staff.ustc.edu.cn/~xyfeng/research/cos/resources/machine/mem.htm
  return mpsearch1(0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
// MPコンフィグレーションテーブルを探す。
// 現時点ではデフォルト設定(物理アドレス==0)は受け付けない
// 現在のシグネチャを確認し、チェックサムを算出し
// もし正しければ、バージョンを確認する
// To do: 拡張テーブルのチェックサムを確認する
static struct mpconf*
mpconfig(struct mp **pmp)
{
  struct mpconf *conf;
  struct mp *mp;

  // MP Floating Pointer Structureが見つからない、もしくはMPコンフィグレーションテーブルの物理アドレスが0の場合
  if((mp = mpsearch()) == 0 || mp->physaddr == 0)
    return 0;
  
  // MPコンフィグレーションテーブルの仮想アドレスを取得
  conf = (struct mpconf*) P2V((uint) mp->physaddr);

  // シグネチャを確認
  if(memcmp(conf, "PCMP", 4) != 0)
    return 0;

    
  if(conf->version != 1 && conf->version != 4)
    return 0;
  if(sum((uchar*)conf, conf->length) != 0)
    return 0;
  *pmp = mp;
  return conf;
}

void
mpinit(void)
{
  uchar *p, *e;
  int ismp;
  struct mp *mp;
  struct mpconf *conf;
  struct mpproc *proc;
  struct mpioapic *ioapic;

  if((conf = mpconfig(&mp)) == 0)
    panic("Expect to run on an SMP");
  
  ismp = 1;
  lapic = (uint*)conf->lapicaddr;


  for(p=(uchar*)(conf+1), e=(uchar*)conf+conf->length; p<e; ){
    switch(*p){
    case MPPROC:
      proc = (struct mpproc*)p;
      if(ncpu < NCPU) {
        cpus[ncpu].apicid = proc->apicid;  // apicid may differ from ncpu
        ncpu++;
      }
      p += sizeof(struct mpproc);
      continue;
    case MPIOAPIC:
      ioapic = (struct mpioapic*)p;
      ioapicid = ioapic->apicno;
      p += sizeof(struct mpioapic);
      continue;
    case MPBUS:
    case MPIOINTR:
    case MPLINTR:
      p += 8;
      continue;
    default:
      ismp = 0;
      break;
    }
  }

  
  if(!ismp)
    panic("Didn't find a suitable machine");

  if(mp->imcrp){
    // Bochs doesn't support IMCR, so this doesn't run on Bochs.
    // But it would on real hardware.
    outb(0x22, 0x70);   // Select IMCR
    outb(0x23, inb(0x23) | 1);  // Mask external interrupts.
  }
}
