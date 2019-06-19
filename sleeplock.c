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

void
initsleeplock(struct sleeplock *lk, char *name)
{
  initlock(&lk->lk, "sleep lock");
  lk->name = name;
  lk->locked = 0;
  lk->pid = 0;
}

void
acquiresleep(struct sleeplock *lk)
{
  acquire(&lk->lk);
  while (lk->locked) {
    sleep(lk, &lk->lk);
  }
  lk->locked = 1;
  lk->pid = myproc()->pid;
  release(&lk->lk);
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
int
holdingsleep(struct sleeplock *lk)
{
  int r;
  
  acquire(&lk->lk); // スピンロックを取得
  r = lk->locked && (lk->pid == myproc()->pid); // ロックを保持しているがカレントプロセスであるかどうか
  release(&lk->lk); // スピンロックを開放
  return r;
}



