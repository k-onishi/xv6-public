// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"


// ロック変数の初期化関数
void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// ロックを取得する
// ロックが取得できるまでスピンロックして待機する
// ロックを長時間保持すると他CPUがスピンロックによる
// 時間の無駄が生じる可能性がある。
void
acquire(struct spinlock *lk)
{
  pushcli(); // デッドロックを回避するため割り込みを禁止する
  if(holding(lk)) // ロックが取得されているかどうか
    panic("acquire");

  // The xchg is atomic.
  while(xchg(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that the critical section's memory
  // references happen after the lock is acquired.
  __sync_synchronize();

  // Record info about lock acquisition for debugging.
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

// Release the lock.
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  // Tell the C compiler and the processor to not move loads or stores
  // past this point, to ensure that all the stores in the critical
  // section are visible to other cores before the lock is released.
  // Both the C compiler and the hardware may re-order loads and
  // stores; __sync_synchronize() tells them both not to.
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // This code can't use a C assignment, since it might
  // not be atomic. A real OS would use C atomics here.
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli();
}

// Record the current call stack in pcs[] by following the %ebp chain.
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // saved %eip
    ebp = (uint*)ebp[0]; // saved %ebp
  }
  for(; i < 10; i++)
    pcs[i] = 0;
}

// 当CPUがロックを保持しているかを確認する
int
holding(struct spinlock *lock)
{
  int r;
  pushcli(); // 割り込みの禁止
  r = lock->locked && lock->cpu == mycpu(); // CPUが同じで且つロックされているかどうか
  popcli(); // 割り込み許可
  return r; // ロックが取得されているかどうか
}

// pushcli/popcliはマッチする以外はcli/sti命令と同様の動作をする命令である。
// 2回分のpushcliｗ開放するには2回分のpopcliを必要とする。もし割り込みが禁止されていれば
// pushcli, popcli共に割り込みを禁止したままとなる。
void
pushcli(void)
{
  int eflags;

  eflags = readeflags(); // EFLAGSレジスタの値を取得
  cli(); // 割り込みの禁止
  if(mycpu()->ncli == 0) // 割り込みが許可されている場合
    mycpu()->intena = eflags & FL_IF; // 
  mycpu()->ncli += 1; // ncliが1以上で割り込み禁止
}

void
popcli(void)
{
  if(readeflags()&FL_IF) // 既に割り込みが許可されている場合
    panic("popcli - interruptible");
  if(--mycpu()->ncli < 0) // -1になることはない
    panic("popcli");
  // 入れ子になって割り込みが禁止されていたのが全て許可された且つ割り込みが許可されている場合
  if(mycpu()->ncli == 0 && mycpu()->intena) 
    sti(); // 割り込みを許可する。
}

