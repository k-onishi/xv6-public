#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# test

# CPUを起動後、32bitのプロテクトモードに遷移しC言語のコードに飛ぶ。
# BIOSはハードディスク上の先頭セクタにあるこのコードを物理アドレスの0x7c00に読み込み
# %cs=0, %ip=7c00でCPUのリアルモードから実行を開始する。

.code16                       # 16-bit mode
.globl start
start:
  cli                         # 割り込みを禁する

  # DS、ES及びSSデータセグメントレジスタをゼロで初期化
  xorw    %ax,%ax             # axにゼロを設定
  movw    %ax,%ds             # データセグメント
  movw    %ax,%es             # エクストラセグメント
  movw    %ax,%ss             # スタックセグメント

  # Physical address line A20 is tied to zero so that the first PCs 
  # with 2 MB would run software that assumed 1 MB.  Undo that.
  # https://en.wikipedia.org/wiki/A20_line
  # http://softwaretechnique.jp/OS_Development/kernel_development10.html
seta20.1:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.1

  movb    $0xd1,%al               # "0xd1"ポート"0x64"に書き込むことでアウトプットポートへの書き込み操作を指定
  outb    %al,$0x64

seta20.2:
  inb     $0x64,%al               # Wait for not busy
  testb   $0x2,%al
  jnz     seta20.2

  movb    $0xdf,%al               # "0xdf"をポート"0x60"に書き込むことで"A20アドレスライン"を有効化する
  outb    %al,$0x60

  # リアルモードから抜ける。起動用のグローバルディスクリプタテーブル(GDT)で
  # 仮想アドレスから物理アドレスへのマップを作成するメモリマップはこの操作中には変更されない
  # https://en.wikipedia.org/wiki/Control_register
  lgdt    gdtdesc # GDTをロード
  movl    %cr0, %eax # コントロールレジスタ0の値をロード
  orl     $CR0_PE, %eax # bit0をセットしプロテクトモードを有効化
  movl    %eax, %cr0 # コントロールレジスタ0の値をセット

//PAGEBREAK!
  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # Tell assembler to generate 32-bit code now.
start32:
  # Set up the protected-mode data segment registers
  movw    $(SEG_KDATA<<3), %ax    # Our data segment selector
  movw    %ax, %ds                # -> DS: Data Segment
  movw    %ax, %es                # -> ES: Extra Segment
  movw    %ax, %ss                # -> SS: Stack Segment
  movw    $0, %ax                 # Zero segments not ready for use
  movw    %ax, %fs                # -> FS
  movw    %ax, %gs                # -> GS

  # Set up the stack pointer and call into C.
  movl    $start, %esp
  call    bootmain

  # If bootmain returns (it shouldn't), trigger a Bochs
  # breakpoint if running under Bochs, then loop.
  movw    $0x8a00, %ax            # 0x8a00 -> port 0x8a00
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax            # 0x8ae0 -> port 0x8a00
  outw    %ax, %dx
spin:
  jmp     spin

# Bootstrap GDT
.p2align 2                                # force 4 byte alignment
gdt:
  SEG_NULLASM                             # null seg
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # code seg
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # data seg

gdtdesc:
  .word   (gdtdesc - gdt - 1)             # sizeof(gdt) - 1
  .long   gdt                             # address gdt

