// This file contains definitions for the
// x86 memory management unit (MMU).

// Eflags register
#define FL_IF           0x00000200      // 割り込み許可状態

// Control Register flags
#define CR0_PE          0x00000001      // Protection Enable
#define CR0_WP          0x00010000      // Write Protect
#define CR0_PG          0x80000000      // Paging

#define CR4_PSE         0x00000010      // 拡張ページング

// セグメントセレクタ
#define SEG_KCODE 1  // カーネルコード
#define SEG_KDATA 2  // カーネルデータ及びスタック
#define SEG_UCODE 3  // ユーザコード
#define SEG_UDATA 4  // ユーザデータ及びスタック
#define SEG_TSS   5  // プロセスのTSS(Task State Segment)

// cpu->gdt[NSEGS]で上で定義されているセグメントを保持する
#define NSEGS     6 // セグメントの数

#ifndef __ASSEMBLER__
// セグメントディスクリプタの構造体
struct segdesc {
  uint lim_15_0 : 16;  // セグメントのサイズ(20bit)の内の下位16bit
  uint base_15_0 : 16; // セグメントベースアドレスの下位16bit
  uint base_23_16 : 8; // セグメントベースアドレスの中間8bit
  uint type : 4;       // セグメントタイプ (see STS_ constants)
  uint s : 1;          // 0 = システム, 1 = アプリケーション
  uint dpl : 2;        // DPL(Descriptor Privilege Level): ディスクリプタ権限レベル
  uint p : 1;          // 存在しているかどうか
  uint lim_19_16 : 4;  // セグメントのサイズ(20bit)の内の上位4bit
  uint avl : 1;        // 未使用(available for software use)
  uint rsv1 : 1;       // 予約済み
  uint db : 1;         // 0 = 16bitセグメント, 1 = 32bitセグメント
  uint g : 1;          // 粒度: 0 = セグメント長はバイト単位, 1 = セグメント長は4Kバイト単位で表現される
  uint base_31_24 : 8; // セグメントベースアドレスの上位8bit
};

// Normal segment
#define SEG(type, base, lim, dpl) (struct segdesc)    \
{ ((lim) >> 12) & 0xffff, (uint)(base) & 0xffff,      \
  ((uint)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint)(lim) >> 28, 0, 0, 1, 1, (uint)(base) >> 24 }
#define SEG16(type, base, lim, dpl) (struct segdesc)  \
{ (lim) & 0xffff, (uint)(base) & 0xffff,              \
  ((uint)(base) >> 16) & 0xff, type, 1, dpl, 1,       \
  (uint)(lim) >> 16, 0, 0, 1, 0, (uint)(base) >> 24 }
#endif

#define DPL_USER    0x3     // User DPL

// アプリケーションセグメントの種類
#define STA_X       0x8     // 実行可能
#define STA_W       0x2     // 書き込み可能 (実行不可)
#define STA_R       0x2     // 読み込み可能 (実行可能)

// システムセグメントの種類
#define STS_T32A    0x9     // Available 32-bit TSS
#define STS_IG32    0xE     // 32bitの割り込みゲート
#define STS_TG32    0xF     // 32bitのトラップゲート

// 仮想アドレスである'la'は次の3のパートから構成されている
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |      Index     |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(va) --/ \--- PTX(va) --/

// ページディレクトリのインデックス(リニアアドレスの上位10bit)
#define PDX(va)         (((uint)(va) >> PDXSHIFT) & 0x3FF)

// ページテーブルのインデックス(リニアアドレスの13~22bit)
#define PTX(va)         (((uint)(va) >> PTXSHIFT) & 0x3FF)

// インデックスとオフセットから仮想アドレスを構築する
#define PGADDR(d, t, o) ((uint)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// Page directory and page table constants.
#define NPDENTRIES      1024    // # ページディレクトリ内の全てのエントリ
#define NPTENTRIES      1024    // # ページテーブル内のエントリ数
#define PGSIZE          4096    // ページサイズ(Byte)

#define PTXSHIFT        12      // リニアアドレス内のページテーブルのオフセット
#define PDXSHIFT        22      // リニアアドレス内のページディレクトリのオフセット

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1)) // ページ境界になるよう値を切り上げる(サイズが2ページ以上、3ページ未満の場合、3ページ分の値に切り上げる)
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1)) // ページ境界になるよう値を切り下げる(サイズが2ページ以上、3ページ未満の場合、2ページ分の値に切り下げる)

// Page table/directory entry flags.
#define PTE_P           0x001   // 存在する
#define PTE_W           0x002   // 書き込み可能
#define PTE_U           0x004   // ユーザ
#define PTE_PS          0x080   // ページサイズ

// Address in page table or page directory entry
#define PTE_ADDR(pte)   ((uint)(pte) & ~0xFFF) // ページテーブルもしくはページディレクトリのエントリ内のアドレス(上位20bit)
#define PTE_FLAGS(pte)  ((uint)(pte) &  0xFFF) // ページテーブルもしくはページディレクトリのフラグ値を取得

#ifndef __ASSEMBLER__
typedef uint pte_t;

// タスクステートセグメントのフォーマット(Task State Segment)
struct taskstate {
  uint link;         // Old ts selector
  uint esp0;         // Stack pointers and segment selectors
  ushort ss0;        //   after an increase in privilege level
  ushort padding1;
  uint *esp1;
  ushort ss1;
  ushort padding2;
  uint *esp2;
  ushort ss2;
  ushort padding3;
  void *cr3;         // Page directory base
  uint *eip;         // Saved state from last task switch
  uint eflags;
  uint eax;          // More saved state (registers)
  uint ecx;
  uint edx;
  uint ebx;
  uint *esp;
  uint *ebp;
  uint esi;
  uint edi;
  ushort es;         // Even more saved state (segment selectors)
  ushort padding4;
  ushort cs;
  ushort padding5;
  ushort ss;
  ushort padding6;
  ushort ds;
  ushort padding7;
  ushort fs;
  ushort padding8;
  ushort gs;
  ushort padding9;
  ushort ldt;
  ushort padding10;
  ushort t;          // Trap on task switch
  ushort iomb;       // I/O map base address
};

// Gate descriptors for interrupts and traps
struct gatedesc {
  uint off_15_0 : 16;   // low 16 bits of offset in segment
  uint cs : 16;         // code segment selector
  uint args : 5;        // # args, 0 for interrupt/trap gates
  uint rsv1 : 3;        // reserved(should be zero I guess)
  uint type : 4;        // type(STS_{IG32,TG32})
  uint s : 1;           // must be 0 (system)
  uint dpl : 2;         // descriptor(meaning new) privilege level
  uint p : 1;           // Present
  uint off_31_16 : 16;  // high bits of offset in segment
};

// Set up a normal interrupt/trap gate descriptor.
// - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate.
//   interrupt gate clears FL_IF, trap gate leaves FL_IF alone
// - sel: Code segment selector for interrupt/trap handler
// - off: Offset in code segment for interrupt/trap handler
// - dpl: Descriptor Privilege Level -
//        the privilege level required for software to invoke
//        this interrupt/trap gate explicitly using an int instruction.
#define SETGATE(gate, istrap, sel, off, d)                \
{                                                         \
  (gate).off_15_0 = (uint)(off) & 0xffff;                \
  (gate).cs = (sel);                                      \
  (gate).args = 0;                                        \
  (gate).rsv1 = 0;                                        \
  (gate).type = (istrap) ? STS_TG32 : STS_IG32;           \
  (gate).s = 0;                                           \
  (gate).dpl = (d);                                       \
  (gate).p = 1;                                           \
  (gate).off_31_16 = (uint)(off) >> 16;                  \
}

#endif
