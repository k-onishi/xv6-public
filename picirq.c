#include "types.h"
#include "x86.h"
#include "traps.h"

// I/O Addresses of the two programmable interrupt controllers
// 2つのプログラマブル割り込みコントローラのI/Oアドレス
#define IO_PIC1         0x20    // Master (IRQs 0-7)
#define IO_PIC2         0xA0    // Slave (IRQs 8-15)

// Don't use the 8259A interrupt controllers.  Xv6 assumes SMP hardware.
// 8259Aの割り込みコントローラを使用しない。Xv6はSMPハードウェアを想定している。
void
picinit(void)
{
  // 全ての割り込みをマスク
  // https://wiki.osdev.org/8259_PIC
  outb(IO_PIC1+1, 0xFF);
  outb(IO_PIC2+1, 0xFF);
}

//PAGEBREAK!
// Blank page.
