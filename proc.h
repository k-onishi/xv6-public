// CPUの状態(各CPU毎に定義される)
struct cpu {
  uchar apicid;                // ローカルAPICのID
  struct context *scheduler;   // ここでコンテキストスイッチが起こりスケジューラに入る
  struct taskstate ts;         // 割り込み用のスタックを見つけるためにx86で用いられる
  struct segdesc gdt[NSEGS];   // x86のグローバルディスクリプタテーブル
  volatile uint started;       // CPUが起動しているかどうか
  int ncli;                    // pushcliネスト数
  int intena;                  // pushcliの前段階で割り込みが可能かどうか?
  struct proc *proc;           // このプロセッサで動作しているプロセスまたはNULL
};

extern struct cpu cpus[NCPU]; // CPU個数分定義される(最大8)
extern int ncpu;

// カーネルのコンテキストスイッチに用いられる保存されたレジスタの値。
// 全てのセグメントレジスタはカーネルコンテキストをまたいで定数なので保存する必要はない
// %eaxや%ecx, %edxは、x86では呼び出し側が保存することになっているため保存する必要はない。
// コンテキストはスタックに保存され、スタックポインタはコンテキストのアドレスだと説明される。
// コンテキストのレイアウトは`swtch.S`の`Switch stacks`のコメントにあるスタックレイアウトに対応する。
// SwitchはEIPを明示的には保存しないが、スタック上にあり`allocproc()`関数が操作する。
struct context {
  uint edi; // Destination Index
  uint esi; // Source Index
  uint ebx; // Base Register
  uint ebp; // Base Pointer
  uint eip; // Instruction Pointer
};

// プロセスの状態
// UNUSED: 使用されていない
// EMBRYO: ??
// SLEEPING: スリープ
// RUNNABLE: 待ち(動作可能)
// RUNNING: 動作中
// ZOMBIE: ゾンビ
enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// プロセスの状態
struct proc {
  uint sz;                     // プロセスのメモリサイズ(bytes)
  pde_t* pgdir;                // ページテーブル(ページディレクトリのエントリ)
  char *kstack;                // 当該プロセスのカーネルスタックの底
  enum procstate state;        // プロセスのステート
  int pid;                     // プロセスID
  struct proc *parent;         // 親プロセス
  struct trapframe *tf;        // システムコールのためのスタックフレーム
  struct context *context;     // プロセスを走らせるためここでswtch()が呼ばれる
  void *chan;                  // 0でない場合chan上でスリープしている
  int killed;                  // 0でない場合KILLされている
  struct file *ofile[NOFILE];  // プロセスがオープン可能なファイル数
  struct inode *cwd;           // カレントディレクトリ
  char name[16];               // プロセス名(デバッグ用)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
