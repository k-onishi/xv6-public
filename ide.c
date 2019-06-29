// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512 // セクタサイズ

// ftp://ftp.seagate.com/acrobat/reference/111-1c.pdf
#define IDE_BSY       0x80 // BUSY
#define IDE_DRDY      0x40 // DRIVE READY
#define IDE_DF        0x20 // DRIVE WRITE FAULT
#define IDE_ERR       0x01 // ERROR

#define IDE_CMD_READ  0x20 // 読み込み
#define IDE_CMD_WRITE 0x30 // 書き込み
#define IDE_CMD_RDMUL 0xc4 // 読み込み(複数)
#define IDE_CMD_WRMUL 0xc5 // 書き込み(複数)

// idequeue points to the buf now being read/written to the disk.
// idequeue->qnext points to the next buf to be processed.
// You must hold idelock while manipulating queue.

static struct spinlock idelock;
static struct buf *idequeue;

static int havedisk1;
static void idestart(struct buf*);

// IDEディスクが準備完了になるまで待機
static int
idewait(int checkerr)
{
  int r;

  // Data Register(0x1F7)がマッピングされているポートからデータを読み込み
  // DRIVE READYの状態でなければ繰り返す
  while(((r = inb(0x1f7)) & (IDE_BSY|IDE_DRDY)) != IDE_DRDY)
    ;
  
  // エラーチェックが指定されており、書き込みエラーまたは一般的なエラーが発生している場合
  if(checkerr && (r & (IDE_DF|IDE_ERR)) != 0)
    return -1;
  return 0;
}

// IDE用のロック変数の初期化及びSlaveドライブの存在を確認
void
ideinit(void)
{
  int i;

  // IDE用のロック変数を初期化
  initlock(&idelock, "ide");

  // IDEの割り込みを一番大きい番号のCPUに割り当て
  ioapicenable(IRQ_IDE, ncpu - 1);
  
  idewait(0); // エラーチェックなしでディスクの状態がDRIVE READYになるまで待機

  // IDENTIFY command
  // "IDENTIFY"コマンド(0xEC)をコマンドI/Oポート(0x1F7)に送信し
  // ステータスポート(0x1F7)から再度値を読み込む
  // 値が0ならドライブは存在しない
  outb(0x1f6, 0xe0 | (1<<4)); // Slaveのドライブの存在を確認する
  for(i=0; i<1000; i++){ // おそらく値が反映されるまでの時間がかかるため(知らんけどよくあるやん)
    if(inb(0x1f7) != 0){ // 存在する場合
      havedisk1 = 1;
      break;
    }
  }

  // Masterのドライブに値を戻す
  outb(0x1f6, 0xe0 | (0<<4));
}

// バッファのためのリクエストを開始する。
// 呼び出し側はideのロックを取得しておく必要がある
static void
idestart(struct buf *b)
{
  // バッファキャッシュの指定なし
  if(b == 0)
    panic("idestart");
  
  // ブロック番号が無効である場合
  if(b->blockno >= FSSIZE)
    panic("incorrect blockno");
  
  // 単一ブロック内のセクタ数の算出及びセクタ番号の算出
  int sector_per_block =  BSIZE/SECTOR_SIZE; // ブロック内のセクタ数(== 1)
  int sector = b->blockno * sector_per_block; // ブロック番号から読み出すセクタの位置を算出

  // セクタサイズとブロックサイズが同じ場合には単一の読み込み
  int read_cmd = (sector_per_block == 1) ? IDE_CMD_READ :  IDE_CMD_RDMUL;
  int write_cmd = (sector_per_block == 1) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

  // セクタ数が多すぎる
  if (sector_per_block > 7) panic("idestart");

  idewait(0); // ディスクが準備完了状態になるまで待機
  // https://wiki.osdev.org/ATA_PIO_Mode
  outb(0x3f6, 0);  // 一般的な割り込み
  outb(0x1f2, sector_per_block);  // セクタ数
  outb(0x1f3, sector & 0xff); // LBAの下位8bit
  outb(0x1f4, (sector >> 8) & 0xff); // LBAの9~16bit
  outb(0x1f5, (sector >> 16) & 0xff); // LBAの17~24bit
  // master == 0xE0, slave == 0xF0
  outb(0x1f6, 0xe0 | ((b->dev&1)<<4) | ((sector>>24)&0x0f)); // LBAの上位4bit(25~28)及び
  if(b->flags & B_DIRTY){ // バッファの書き込みが必要である場合
    outb(0x1f7, write_cmd); // 書き込みコマンドを設定
    outsl(0x1f0, b->data, BSIZE/4); // 実際に書き込み(一度に4バイト送信するためブロックサイズを4で除算する)
  } else {
    outb(0x1f7, read_cmd); // 読み込み
  }
}

// Interrupt handler.
void
ideintr(void)
{
  struct buf *b;

  // First queued buffer is the active request.
  acquire(&idelock);

  if((b = idequeue) == 0){
    release(&idelock);
    return;
  }
  idequeue = b->qnext;

  // Read data if needed.
  if(!(b->flags & B_DIRTY) && idewait(1) >= 0)
    insl(0x1f0, b->data, BSIZE/4);

  // Wake process waiting for this buf.
  b->flags |= B_VALID;
  b->flags &= ~B_DIRTY;
  wakeup(b);

  // Start disk on next buf in queue.
  if(idequeue != 0)
    idestart(idequeue);

  release(&idelock);
}

//PAGEBREAK!
// ディスクとバッファの内容を同期させる
// B_DIRTYがセットされている場合はバッファをディスクに書き込み、B_DIRTYフラグ
// をクリアした後、B_VALIDをセットする
// もしB_VALIDがセットされていない場合にはディスクからバッファを読み込み、B_DIRTYフラグ
// をセットする
void iderw(struct buf *b)
{
  struct buf **pp;

  // バッファのロックがされていない場合
  if(!holdingsleep(&b->lock))
    panic("iderw: buf not locked");
  
  // バッファが有効である場合、何もする必要がない
  if((b->flags & (B_VALID|B_DIRTY)) == B_VALID)
    panic("iderw: nothing to do");

  // disk1を保持していない場合
  if(b->dev != 0 && !havedisk1)
    panic("iderw: ide disk 1 not present");

  // ディスクのロックを行う
  acquire(&idelock);  //DOC:acquire-lock

  b->qnext = 0;
  // リストの末尾へ移動
  for(pp=&idequeue; *pp; pp=&(*pp)->qnext)  //DOC:insert-queue
    ;
  *pp = b; // バッファキャッシュをideキューの最後尾へ

  // 必要であればディスクを起動する
  if(idequeue == b) // バッファキャッシュがideキューの先頭要素だった場合
    idestart(b);

  // リスクエストが完了するまで待機
  while((b->flags & (B_VALID|B_DIRTY)) != B_VALID){ // バッファが有効になるまで待機
    sleep(b, &idelock); // 待機
  }

  release(&idelock); // IDEのロックを開放
}
