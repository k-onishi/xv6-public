// Sleeping locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

// スリープロックの初期化
void initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

// ロックが取得できるまでスリープして待つ(カレントプロセス)
void acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk); // ロックを取得する
  while (lk->locked) { // ロックされている間
    sleep(lk, &lk->lk); // カレントプロセスをスリープさせる
  }
  lk->locked = 1; // ロックが取得されている状態を記録
  lk->pid = myproc()->pid; // ロックを取得しているプロセスのPID
  release(&lk->lk); // ロックを開放する
}

// スリープロックを開放し、そのロック上でスリープしているプロセスを起床させる
void releasesleep(struct sleeplock *lk)
{
  acquire(&lk->lk); // スピンロックを取得
  lk->locked = 0; // ロックを開放
  lk->pid = 0; // PIDをクリア
  wakeup(lk); // スリープロック上でスリープしているプロセスを起床させる
  release(&lk->lk); // スピンロックを開放
}

// カレントプロセスがスリープロック(長い期間のロック)を保持しているかどうか
int holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk); // スピンロックを取得
  r = lk->locked && (lk->pid == myproc()->pid); // ロックを保持しているがカレントプロセスであるかどうか
  release(&lk->lk); // スピンロックを開放
  return r;
}



