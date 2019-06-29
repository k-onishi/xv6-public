// Routines to let C code use special x86 instructions.

// 指定のポートからデータを読み込む
static inline uchar inb(ushort port)
{
  uchar data;

  // https://c9x.me/x86/html/file_module_x86_id_139.html
  asm volatile("in %1,%0" : "=a" (data) : "d" (port));
  return data;
}

/**
 * 指定したI/Oポートから指定したアドレスにセクタのデータを読み込む
 */
static inline void
insl(int port, void *addr, int cnt)
{
  // cld(clear direction flag)命令: (http://caspar.hazymoon.jp/OpenBSD/annex/intel_arc.html)
  // Direction Flagをクリア("0")する事で指定アドレスから昇順にデータを読み込む("1"の場合は降順)
  
  // rep(repeat)命令
  // "rep"は命令修飾子で後に続く命令をecxレジスタ値の回数繰り返し実行する(今回の場合はinsl命令が対象)
  
  // in, ins(input from port)命令(今回はinsl(ins long)のため一度に32bit読み込む)
  // edxレジスタで指定したI/Oポートからediレジスタで指定したアドレスの始点にecxレジスタ値の回数分データを読み込む
  asm volatile("cld; rep insl" :
               "=D" (addr), "=c" (cnt) :
               "d" (port), "0" (addr), "1" (cnt) :
               "memory", "cc");
}

// 指定のI/Oポートにデータ(1バイト)を書き込む
static inline void outb(ushort port, uchar data)
{
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

// 指定のI/Oポートにデータ(2バイト)を書き込む
static inline void
outw(ushort port, ushort data)
{
  asm volatile("out %0,%1" : : "a" (data), "d" (port));
}

// 指定ポートに指定アドレスからのデータ(4バイト)を指定回数書き込む
static inline void
outsl(int port, const void *addr, int cnt)
{
  asm volatile("cld; rep outsl" :
               "=S" (addr), "=c" (cnt) :
               "d" (port), "0" (addr), "1" (cnt) :
               "cc");
}

// 指定アドレスを始点に指定したデータを指定回数バイト分、1バイトずつ設定する
static inline void
stosb(void *addr, int data, int cnt)
{
  // stosb(store string byte)命令: alレジスタの値をdiレジスタで指定したアドレスを始点にecxレジスタ値の回数、始点から1バイトずつ設定する
  asm volatile("cld; rep stosb" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

static inline void
stosl(void *addr, int data, int cnt)
{
  asm volatile("cld; rep stosl" :
               "=D" (addr), "=c" (cnt) :
               "0" (addr), "1" (cnt), "a" (data) :
               "memory", "cc");
}

struct segdesc;

// GDTRをlgdt命令でセットする
static inline void
lgdt(struct segdesc *p, int size)
{
  volatile ushort pd[3];

  // セグメントディスクリプタのアドレス及びサイズを配列に設定
  pd[0] = size-1;
  pd[1] = (uint)p;
  pd[2] = (uint)p >> 16;

  // http://caspar.hazymoon.jp/OpenBSD/annex/intel_segment.html
  // lgdt(Load GDT)命令でGDTR(Global Descriptor Table Register)をセットする
  asm volatile("lgdt (%0)" : : "r" (pd));
}

struct gatedesc;

// LIDT(Load Interrupt Descriptor Table)命令でIDTR(Interrupt Descriptor Table Register)に
// IDT(Interrupt Descriptor Table)のサイズ及びアドレスを設定する
// http://softwaretechnique.jp/OS_Development/kernel_development02.html
static inline void
lidt(struct gatedesc *p, int size)
{
  volatile ushort pd[3];

  pd[0] = size-1; // サイズ

  // アドレスを16bitずつ代入
  pd[1] = (uint)p;
  pd[2] = (uint)p >> 16;

  // 設定
  asm volatile("lidt (%0)" : : "r" (pd));
}

static inline void
ltr(ushort sel)
{
  asm volatile("ltr %0" : : "r" (sel));
}

// EFLAGSレジスタの値を取得する
static inline uint
readeflags(void)
{
  uint eflags;
  asm volatile("pushfl; popl %0" : "=r" (eflags));
  return eflags;
}

static inline void
loadgs(ushort v)
{
  asm volatile("movw %0, %%gs" : : "r" (v));
}

// 割り込みの禁止
static inline void
cli(void)
{
  asm volatile("cli");
}

// 割り込みを許可
static inline void
sti(void)
{
  asm volatile("sti");
}

// アドレスで指定した値とnewvalを入れ替える
static inline uint
xchg(volatile uint *addr, uint newval)
{
  uint result;

  // "+m"の"+"は読み込み/変更/書き込みを行うオペランドであることを示す
  // lockは後続のオペコードをアトミックに処理するためのプレフィクス
  // xchg: http://softwaretechnique.jp/OS_Development/Tips/IA32_Instructions/XCHG.html
  asm volatile("lock; xchgl %0, %1" : // 値を交換する(どちらか一方がメモリアドレスだった場合自動的に"lock"プレフィクスが付与される)
               "+m" (*addr), "=a" (result) :
               "1" (newval) :
               "cc"); // ステータスレジスタが更新される可能性があるの意
  return result;
}

static inline uint
rcr2(void)
{
  uint val;
  asm volatile("movl %%cr2,%0" : "=r" (val));
  return val;
}

// cr3にページディレクトリorページテーブルの物理アドレスを設定する。
static inline void
lcr3(uint val)
{
  asm volatile("movl %0,%%cr3" : : "r" (val));
}

// trapasm.Sによってハードウェアのスタック上で構築され、trap()関数に
// 渡されるトラップフレームのレイアウト。
struct trapframe {
  // "pusha"によってプッシュされるレジスタ群
  uint edi;
  uint esi;
  uint ebp;
  uint oesp;      // 不必要で無視される
  uint ebx;
  uint edx;
  uint ecx;
  uint eax;

  // 残りのトラップフレーム
  ushort gs;
  ushort padding1;
  ushort fs;
  ushort padding2;
  ushort es;
  ushort padding3;
  ushort ds;
  ushort padding4;
  uint trapno;

  // ここから下はx86のハードウェアで定義されている
  uint err;
  uint eip;
  ushort cs;
  ushort padding5;
  uint eflags;

  // ここから下はユーザモードからカーネルモードへなど、リングレベルをまたぐ
  uint esp;
  ushort ss;
  ushort padding6;
};
