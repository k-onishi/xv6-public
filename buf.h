
struct buf {
  int flags;
  uint dev;
  uint blockno; // ブロック番号
  struct sleeplock lock; // スピンロック用変数
  uint refcnt; // 参照回数
  struct buf *prev; // LRU cache list
  struct buf *next;
  struct buf *qnext; // disk queue
  uchar data[BSIZE];
};
#define B_VALID 0x2  // バッファはディスクから読み込まれたものである
#define B_DIRTY 0x4  // バッファをディスクに書き戻す必要がある。

