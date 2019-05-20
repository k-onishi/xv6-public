// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"

#define SECTSIZE  512 // セクタのサイズ

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  elf = (struct elfhdr*)0x10000;  // scratch space

  // Read 1st page off disk
  readseg((uchar*)elf, 4096, 0);

  // Is this an ELF executable?
  if(elf->magic != ELF_MAGIC)
    return;  // let bootasm.S handle error

  // Load each program segment (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff);
  eph = ph + elf->phnum;
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr;
    readseg(pa, ph->filesz, ph->off);
    if(ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz);
  }

  // Call the entry point from the ELF header.
  // Does not return!
  entry = (void(*)(void))(elf->entry);
  entry();
}

// 命令の送受信が可能になるまで待機する(これを待たないとハングアップする可能性がある)
void
waitdisk(void)
{
  // https://wiki.osdev.org/ATA_PIO_Mode#Primary.2FSecondary_Bus
  // 0x1F7: Status Register
  // - 0	ERR	Indicates an error occurred. Send a new command to clear it (or nuke it with a Software Reset).
  // - 1	IDX	Index. Always set to zero.
  // - 2	CORR	Corrected data. Always set to zero.
  // - 3	DRQ	Set when the drive has PIO data to transfer, or is ready to accept PIO data.
  // - 4	SRV	Overlapped Mode Service Request.
  // - 5	DF	Drive Fault Error (does not set ERR).
  // - 6	RDY	Bit is clear when drive is spun down, or after an error. Set otherwise.
  // - 7	BSY	Indicates the drive is preparing to send/receive data (wait for it to clear). In case of 'hang' (it never clears), do a software reset.
  //
  // 0xC0: 命令の送受信が可能どうか。
  while((inb(0x1F7) & 0xC0) != 0x40);
}

// offsetで指定したセクタを読み込みdstに書き込む
void
readsect(void *dst, uint offset)
{
  // x86 Directions - 28 bit PIO :https://wiki.osdev.org/ATA_PIO_Mode#Primary.2FSecondary_Bus#x86_Directions
  waitdisk(); // 命令の送受信可能になるまで待つ
  outb(0x1F2, 1);   // セクタの数
  outb(0x1F3, offset); 
  outb(0x1F4, offset >> 8);
  outb(0x1F5, offset >> 16);
  outb(0x1F6, (offset >> 24) | 0xE0);
  outb(0x1F7, 0x20);  // cmd 0x20 - read sectors

  // Read data.
  waitdisk(); // 命令の送受信可能になるまで待つ
  insl(0x1F0, dst, SECTSIZE/4);
}

// オフセット("offset")で指定したセクタから"count"バイト分のデータを読み込み、指定の物理アドレス"pa"に書き込む。
// おそらく指定したよりも多くの読み込みが発生する。
void
readseg(uchar* pa, uint count, uint offset)
{
  uchar* epa;
  epa = pa + count;

  // セクタ境界で丸める
  pa -= offset % SECTSIZE;

  // オフセットをバイトからセクタサイズの単位へ変換(カーネルはセクタ"1"から始まる)
  offset = (offset / SECTSIZE) + 1;

  // もしこの操作が遅い場合、一度の書き込みで多くの読み込みを行なっているかもしれない
  // 指定したよりも多く書き込むがコレは重要ではない。ロードは昇順で行う。
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset);
}
