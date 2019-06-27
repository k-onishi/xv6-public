// Intel 8250 serial port (UART).

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

// http://philipstorr.id.au/pcbook/book2/serial.htm
// COMポート1がI/Oアドレスの0x3FB~0x3FFに割り当てられる(IRQは4)
#define COM1    0x3f8 // UART

// UARTが存在するかどうか
static int uart;

// UARTの初期化
// https://docs.freebsd.org/doc/3.0-RELEASE/usr/share/doc/ja/handbook/cy.html
void
uartinit(void)
{
  char *p;

  // Turn off the FIFO
  // FIFO Control Register (FCR)
  outb(COM1+2, 0); // Bit 0: 16550 FIFO Enable (無効化)

  // 9600 baud, 8 data bits, 1 stop bit, parity off.

  // Line Control Register (LCR)
  outb(COM1+3, 0x80);
  // Bit 7: Divisor Latch Access Bit (DLAB)
  // が設定されている場合, transmit/receive
	// register (THR/RBR) と Interrupt Enable
	// Register (IER) へのアクセスがすべてのア
	// クセスは Divisor Latch Register へリダ
	// イレクトされます.

  // 周波数の設定
  outb(COM1+0, 115200/9600); // Divisor Latch LSB (DLL) (DLAB==1)
  outb(COM1+1, 0); // Divisor Latch MSB (DLH) (DLAB==1)

  // Line Control Register (LCR)
  outb(COM1+3, 0x03); // データ長を8bitに設定

  // Modem Control Register (MCR)
  outb(COM1+4, 0);

  // Interrupt Enable Register (IER)
  outb(COM1+1, 0x01); // Enable Received Data Available Interrupt (ERBFI)
  // FIFO が無効の場合にシグナル文字が受信された時にUARTが割り込みを生成する

  // Line Status Register (LSR)
  // もし読み込んだ値が0xFFの場合はシリアルポートではない
  if(inb(COM1+5) == 0xFF)
    return;
  
  uart = 1; // UARTが存在する

  // 既に存在している割り込みの状態を認識する
  inb(COM1+2);
  inb(COM1+0);

  // シリアルポート割り込みを有効化し、CPUの0番にルーティングする
  ioapicenable(IRQ_COM1, 0);

  // ここまで処理が進行していることを示す
  // "xv6...\n"を出力する
  for(p="xv6...\n"; *p; p++)
    uartputc(*p);
}

// COMポート1から指定のデータを送信する
void
uartputc(int c)
{
  int i;

  // UARTが存在しない場合には何もせず
  if(!uart)
    return;
  
  // https://docs.freebsd.org/doc/3.0-RELEASE/usr/share/doc/ja/handbook/cy.html
  // iが128以下且つTransmitter Holding Register Empty (THRE)が0である
  for(i = 0; i < 128 && !(inb(COM1+5) & 0x20); i++)
    microdelay(10); // 待機
  
  // Transmit Holding Register (THR)
  // このポートに書き込まれた情報はデータ命令として処理されUARTにより送信されます.
  outb(COM1+0, c);
}

static int
uartgetc(void)
{
  if(!uart)
    return -1;
  if(!(inb(COM1+5) & 0x01))
    return -1;
  return inb(COM1+0);
}

void
uartintr(void)
{
  consoleintr(uartgetc);
}
