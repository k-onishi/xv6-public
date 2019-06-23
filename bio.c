// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID: the buffer data has been read from the disk.
// * B_DIRTY: the buffer data has been modified
//     and needs to be written to disk.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock; // ロック用の変数
  struct buf buf[NBUF]; // バッファキャッシュのリスト

  // 全てのバッファの双方向リスト
  // head.nextが一番最近使用したものになる
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

//PAGEBREAK!
  // Create linked list of buffers
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// デバイスのブロックがバッファキャッシュに存在する確認し
// もしなければバッファを割り当てる
// もし存在すればロックされたバッファを返す。
static struct buf* bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock); // バッファキャッシュリスト用のロックを取得

  // Is the block already cached?
  // ブロックキャッシュのリストをトラバースしていく
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    // デバイス番号及びブロック番号が同じ場合
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++; // 参照回数をインクリメント
      release(&bcache.lock); // ロックを開放
      acquiresleep(&b->lock); // バッファをロックできるまで待機
      return b; // バッファキャッシュを返す
    }
  }

  // キャッシュされていない場合、使用されていないバッファをリサイクルする。
  // 参照カウンタ(refcnt)が0であっても、flagsに"B_DIRTY"が設定されている場合は
  // バッファは使用中であることを示す、なせなら
  // log.cは変更されているがコミットしていないためである。
  // バッファキャッシュのリストを使用頻度の低い順にトラバースしていく
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    // 参照カウンタが0且つflagsにB_DIRTYが設定されていない == 使用されていない
    if(b->refcnt == 0 && (b->flags & B_DIRTY) == 0) {
      b->dev = dev; // デバイス番号
      b->blockno = blockno; // ブロック番号
      b->flags = 0; // フラグをクリア
      b->refcnt = 1; // 参照カウンタを設定
      release(&bcache.lock); // バッファキャッシュのロックを開放
      acquiresleep(&b->lock); // バッファキャッシュリストのロックを取得
      return b; // バッファキャッシュを返す
    }
  }
  panic("bget: no buffers"); // キャッシュが見つからなかった
}

// 指定のブロックデータを保持するバッファをロックされた状態で返す
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno); // バッファキャッシュを取得(既存または新規取得したもの)
  if((b->flags & B_VALID) == 0) { // バッファが有効である場合
    iderw(b); // バッファのデータをディスクに書き出す
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  b->flags |= B_DIRTY;
  iderw(b);
}

// Release a locked buffer.
// Move to the head of the MRU list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}
//PAGEBREAK!
// Blank page.

