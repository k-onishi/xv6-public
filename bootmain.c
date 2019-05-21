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

#define SECTSIZE  512 // セクタサイズ

void readseg(uchar*, uint, uint);

void
bootmain(void)
{
  struct elfhdr *elf;
  struct proghdr *ph, *eph;
  void (*entry)(void);
  uchar* pa;

  // 0x10000にカーネルを読み込む
  elf = (struct elfhdr*)0x10000;  // scratch space

  // 先頭セクタから4KB読み込む
  readseg((uchar*)elf, 4096, 0);

  // magicナンバーからELFフォーマットであるかを判定
  if(elf->magic != ELF_MAGIC)
    return; // bootasm.Sのエラーハンドラを実行する

  // 各セグメントの読み込み (ignores ph flags).
  ph = (struct proghdr*)((uchar*)elf + elf->phoff); // プログラムヘッダの開始位置(ELFヘッダのアドレス + プログラムヘッダまでのオフセット)
  eph = ph + elf->phnum; // プログラムヘッダ数を取得
  for(; ph < eph; ph++){
    pa = (uchar*)ph->paddr; // プログラムヘッダの物理アドレス

    // プログラムヘッダの物理アドレスへファイルイメージのサイズ分、オフセットで指定したセクタからデータを読み込む
    readseg(pa, ph->filesz, ph->off);

    // メモリイメージサイズがファイルイメージサイズを上回る場合、はみ出した分を"0"埋めする
    if(ph->memsz > ph->filesz)
      stosb(pa + ph->filesz, 0, ph->memsz - ph->filesz); // 指定のアドレスからはみ出したサイズ分"0"埋めする
  }

  // ELFヘッダのエントリポイントから関数を読み込む
  entry = (void(*)(void))(elf->entry);
  entry(); // ここへは戻ってこない
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
// https://wiki.osdev.org/ATA_PIO_Mode#Primary.2FSecondary_Bus#x86_Directions
void
readsect(void *dst, uint offset)
{
  // 28 bit PIO
  waitdisk(); // 命令の送受信可能になるまで待つ
  
  outb(0x1F2, 1); // 読み込みセクタ数
  
  // 28bitを4回に分けて指定する
  outb(0x1F3, offset); // 最下位8bit
  outb(0x1F4, offset >> 8); // 次の8bit
  outb(0x1F5, offset >> 16); // 次の8bit
  // Send 0xE0 for the "master" or 0xF0 for the "slave",
  outb(0x1F6, (offset >> 24) | 0xE0); // 最上位4bit及び"master"に送信する値("0xE0")
  
  outb(0x1F7, 0x20);  // cmd 0x20 - セクタの読み込みコマンド

  waitdisk(); // 命令の送受信可能になるまで待つ

  // long(32 bit = 4 Byte)単位で送信するためセクタサイズ(Byte)を4で割る
  insl(0x1F0, dst, SECTSIZE/4);
}

// オフセット("offset")で指定したセクタから"count"バイト分のデータを読み込み、指定の物理アドレス"pa"に書き込む。
// おそらく指定したよりも大きなデータの読み込みが発生する。
// readseg((uchar*)elf, 4096, 0); in bootmain()
void
readseg(uchar* pa, uint count, uint offset)
{
  // データの終端アドレス。この"アドレス-1"の位置までデータを読み込む
  uchar* epa;
  epa = pa + count;

  // 読み込み開始位置をセクタ境界で丸める
  pa -= offset % SECTSIZE;

  // オフセットをバイトからセクタサイズ単位へ変換(カーネルはセクタ"1"から始まる)
  offset = (offset / SECTSIZE) + 1;

  // "pa"アドレスを始点に"epa-1"まで、512(SECTSIZE)バイトずつ読み込む
  for(; pa < epa; pa += SECTSIZE, offset++)
    readsect(pa, offset); // offset(LBA)で指定したセクタから読み込んだデータを"pa"アドレスに展開する
}
