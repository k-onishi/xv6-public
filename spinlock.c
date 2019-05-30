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
  while(xchg(&lk->locked, 1) != 0) // スピンロック(値が1である間ループする)
    ;

  // ロックを取得した後クリティカルセクション内のメモリ参照が発生することを保証するため、
  // コンパイラにこれ以前にロードまたはストア命令を並び替えないように指示する。  
  __sync_synchronize(); // メモリバリア

  // Record info about lock acquisition for debugging.
  // デバッグのためロック取得時の情報を記録する
  lk->cpu = mycpu(); // ロックを取得しているCPUを更新
  getcallerpcs(&lk, lk->pcs); // コールトレースのための情報を記録
}

// ロックを開放する
void
release(struct spinlock *lk)
{
  if(!holding(lk)) // ロックが既に開放されている場合
    panic("release");

  // コールスタックの情報をリセット
  lk->pcs[0] = 0;
  lk->cpu = 0;

  // ロックを開放する前にクリティカルセッション内の全てのストア命令が他のプロセッサから見えるように
  // コンパイラとプロセッサ対して最適化によってロードやストア命令をこれより前に並び替えないよう指示する
  // Cコンパイラ及びハードウェアはおそらくロードやストア命令を並び替える。これを__sync_synchronize()によって阻止する
  __sync_synchronize();

  // ロックを開放する、lk->locked = 0と等価。
  // このコードはアトミックに処理されない可能性あるためC言語からは使用できない。
  // 実際のOSではCのアトミックを使用する。
  asm volatile("movl $0, %0" : "+m" (lk->locked) : );

  popcli(); // 割り込み禁止カウンタをデクリメント
}

// ebpを基準に現在のコールスタック(eip)をpcs[]に記録する
void
getcallerpcs(void *v, uint pcs[])
{
  uint *ebp;
  int i;

  ebp = (uint*)v - 2;
  for(i = 0; i < 10; i++){
    // ebpが0またはカーネル仮想アドレスより小さい、またはebpが0xFFFFFFFFの場合
    if(ebp == 0 || ebp < (uint*)KERNBASE || ebp == (uint*)0xffffffff)
      break;
    pcs[i] = ebp[1];     // eipを保存
    ebp = (uint*)ebp[0]; // ebpを保存
  }
  // 記録したeipが10個満たない場合は0埋めする 
  for(; i < 10; i++)
    pcs[i] = 0;
}

// 当CPUがロックを保持しているかを確認する
int
holding(struct spinlock *lock)
{
  int r;
  pushcli(); // 割り込みの禁止
  r = lock->locked && lock->cpu == mycpu(); // CPUがカレントCPUで且つロックされているかどうか
  popcli(); // 割り込み許可
  return r; // ロックが取得されているかどうか
}

// pushcli/popcliはマッチする以外はcli/sti命令と同様の動作をする命令である。
// 2回分のpushcliｗ開放するには2回分のpopcliを必要とする。もし割り込みが禁止されていれば
// pushcli, popcli共に割り込みを禁止したままとなる。

// 割り込み禁止カウンタをインクリメント
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

// 割り込み禁止カウンタをデクリメント
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

