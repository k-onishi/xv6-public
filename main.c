#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers(void);
static void mpmain(void)  __attribute__((noreturn));
extern pde_t *kpgdir;
extern char end[]; // ELFファイルからロードしたカーネルの後ろから続くアドレスの先頭

// 起動用のプロセッサはC言語で記述されたコードをここから開始する。
// 本当のスタックを割り当て、ここへ切り替えを行う。
// 最初にメモリアロケータの動作に必要なセットアップを行う。

int
main(void)
{
  kinit1(end, P2V(4*1024*1024)); // 物理ページアロケータ
  kvmalloc();      // カーネルページテーブルの一部をセットアップする
  mpinit();        // 他のCPUを検出する
  lapicinit();     // 割り込みコントローラの初期化
  seginit();       // セグメントディスクリプタテーブルの設定
  picinit();       // PICの無効化
  ioapicinit();    // I/O APICお初期化
  consoleinit();   // コンソールの初期化
  uartinit();      // UARTの初期化
  pinit();         // プロセステーブル用のロックを初期化
  tvinit();        // 割り込み・トラップゲート及びtick割り込み用ロックの初期化
  binit();         // バッファキャッシュの初期化
  fileinit();      // ファイルテーブル用ロックの初期化
  ideinit();       // IDE用のロック変数及びSlaveドライブの存在確認
  startothers();   // 他のCPUを起動する
  kinit2(P2V(4*1024*1024), P2V(PHYSTOP)); // startothers()の後に呼び出す必要がある
  userinit();      // 最初のユーザプロセス
  mpmain();        // finish this processor's setup
}

// APはentryother.Sからここにジャンプする
static void
mpenter(void)
{
  switchkvm(); // カーネル専用のページテーブルを設定
  seginit(); // セグメントディスクリプタをセットアップ
  lapicinit(); // APICのセットアップ
  mpmain();
}

// 一般的なCPUのセットアップのためのコード
static void
mpmain(void)
{
  // コンソールにCPUが起動することを表示
  cprintf("cpu%d: starting %d\n", cpuid(), cpuid());
  idtinit();       // IDTを設定する
  
  // startothers()関数に対して当該CPUが起動したことを知らせる
  xchg(&(mycpu()->started), 1);
  scheduler(); // プロセスの起動を開始する
}

pde_t entrypgdir[];  // For entry.S

// ブートプロセッサー(Boot Processor: BP)以外のアプリケーションプロセッサ(Application Processor: AP)を起動する
static void
startothers(void)
{
  extern uchar _binary_entryother_start[], _binary_entryother_size[];
  uchar *code;
  struct cpu *c;
  char *stack;

  // エントリのコードを使用していないメモリである0x7000に書き込む
  // リンカがentryother.Sのコードを_binary_entryother_startに配置する
  code = P2V(0x7000); // 仮想アドレスに変換
  // entryother.Sのコードを配置
  memmove(code, _binary_entryother_start, (uint)_binary_entryother_size);

  for(c = cpus; c < cpus+ncpu; c++){
    if(c == mycpu()) // ブートプロセッサは既に起動しているためスキップ
      continue;

    // entryother.Sにどのスタックを使用するか、どこからスタートするか
    // そしてどのページディレクトリを使用するかを指定する。低いメモリで動作しているため
    // まだページディレクトリは使用できないので、"entrypgdir"をAPでも使用する。
    stack = kalloc(); // スタックを確保
    *(void**)(code-4) = stack + KSTACKSIZE; // スタック
    *(void(**)(void))(code-8) = mpenter; // エントリポイント
    *(int**)(code-12) = (void *) V2P(entrypgdir); // ページディスクリプタテーブル

    // APを起動する
    lapicstartap(c->apicid, V2P(code));

    // APのmpmain()の完了を待つ
    while(c->started == 0)
      ;
  }
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte pages.

__attribute__((__aligned__(PGSIZE)))
pde_t entrypgdir[NPDENTRIES] = {
  // Map VA's [0, 4MB) to PA's [0, 4MB)
  [0] = (0) | PTE_P | PTE_W | PTE_PS,
  // Map VA's [KERNBASE, KERNBASE+4MB) to PA's [0, 4MB)
  [KERNBASE>>PDXSHIFT] = (0) | PTE_P | PTE_W | PTE_PS,
};

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

