// The I/O APIC manages hardware interrupts for an SMP system.
// http://www.intel.com/design/chipsets/datashts/29056601.pdf
// See also picirq.c.

#include "types.h"
#include "defs.h"
#include "traps.h"

#define IOAPIC  0xFEC00000   // I/O APICがデフォルトでマッピングされている物理アドレス

#define REG_ID     0x00  // レジスタのインデックス: ID
#define REG_VER    0x01  // レジスタのインデックス: version
#define REG_TABLE  0x10  // リダイレクトテーブルベース

// リダイレクトテーブルは"REG_TABLE"から始まり、各割り込みを2つのレジスタを用いて設定する。
// ペアの最初の(低い)レジスタは設定用のbitを保持している。ペアの2番目のレジスタはどのCPUが割り込みを
// 受け取ることが可能であるかを示すマスク値を保持している。
#define INT_DISABLED   0x00010000  // 割り込み禁止

// http://www.asahi-net.or.jp/~gt3n-tnk/PE6.html
#define INT_LEVEL      0x00008000  // レベル割込み or エッジ割込み
#define INT_ACTIVELOW  0x00002000  // Active low (vs high)
#define INT_LOGICAL    0x00000800  // 割り込みをCPU IDで指定するかAPIC IDで指定するか

// I/O APICがマッピングされたアドレスを設定するためのポインタ
volatile struct ioapic *ioapic;

// IO APICはMMIO(メモリマップドI/O)
struct ioapic {
  uint reg; // regに書き込み、
  uint pad[3];
  uint data; // dataを読み書きする。
};

// I/O APICがマッピングされたアドレスを起点に、指定したインデックス値
// に対応する値を返す。
static uint
ioapicread(int reg)
{
  ioapic->reg = reg; // regに指定した値に対応する値が
  return ioapic->data; // dataから読み込める
}

// I/O APICがマッピングされたアドレスを起点に、指定したインデックス値
// に対応する値を設定する。
static void
ioapicwrite(int reg, uint data)
{
  ioapic->reg = reg; // regに指定した値に対応する値を
  ioapic->data = data; // dataに設定する
}

void
ioapicinit(void)
{
  int i, id, maxintr;

  ioapic = (volatile struct ioapic*)IOAPIC; // I/O APICがマッピングされているアドレスをポインタとして
  maxintr = (ioapicread(REG_VER) >> 16) & 0xFF; // 割り込み
  id = ioapicread(REG_ID) >> 24;
  if(id != ioapicid)
    cprintf("ioapicinit: id isn't equal to ioapicid; not a MP\n");

  // 全ての割り込みをエッジで発行するように、ハイでアクティブに、割り込みを無効に
  // そしてCPUにはルーティングしないよう設定する。
  for(i = 0; i <= maxintr; i++){
    ioapicwrite(REG_TABLE+2*i, INT_DISABLED | (T_IRQ0 + i));
    ioapicwrite(REG_TABLE+2*i+1, 0);
  }
}

// 指定の割り込みを指定のCPUにルーティングするよう設定する
void
ioapicenable(int irq, int cpunum)
{
  // Mark interrupt edge-triggered, active high,
  // enabled, and routed to the given cpunum,
  // which happens to be that cpu's APIC ID.
  // 指定された割り込みを有効にし、エッジで発行、ハイでアクティブ
  // そして指定のCPUに対してルーティングするよう設定する
  ioapicwrite(REG_TABLE+2*irq, T_IRQ0 + irq);
  ioapicwrite(REG_TABLE+2*irq+1, cpunum << 24);
}
