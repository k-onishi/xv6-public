#include "asm.h"
#include "memlayout.h"
#include "mmu.h"

# CPUを起動後、32bitのプロテクトモードに遷移しC言語のコードに飛ぶ。
# BIOSはハードディスク上の先頭セクタにあるこのコードを物理アドレスの0x7c00に読み込み
# %cs=0, %ip=7c00でCPUのリアルモードから実行を開始する。
.code16                       # 16-bitのアセンブリ命令を出力する
.globl start
start:
  cli                         # 割り込みを禁する

  # DS、ES及びSSデータセグメントレジスタをゼロで初期化
  xorw    %ax,%ax             # axにゼロを設定
  movw    %ax,%ds             # データセグメント
  movw    %ax,%es             # エクストラセグメント
  movw    %ax,%ss             # スタックセグメント

  # with 2 MB would run software that assumed 1 MB.  Undo that.
  # 物理アドレスライン"A20"(アドレスバスの20本目以降をマスクするかどうかのフラグ)
  # は0に設定されている。
  # https://en.wikipedia.org/wiki/A20_line
  # https://en.wikipedia.org/wiki/A20_line
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

  # ロングジャンプ(long jmp)を用いて%cs及び%eipをリロードし、32bitのプロテクトモードへの移行が完了する。
  # セグメントディスクリプタは置き換え無しにセットアップされるため、マッピングは変化しない
  # about "ljmp" opecode : https://docs.oracle.com/cd/E19455-01/806-3773/instructionset-73/index.html
  # ljmp $value_for_cs, $value_for_eip
  ljmp    $(SEG_KCODE<<3), $start32

.code32  # 32-bitのアセンブリ命令を出力する
start32:
  # プロテクトモード用のデータセグメントレジスタをセットアップ
  movw    $(SEG_KDATA<<3), %ax    # データセグメントレジスタ
  movw    %ax, %ds                # -> DS: データセグメント
  movw    %ax, %es                # -> ES: エクストラセグメント
  movw    %ax, %ss                # -> SS: スタックセグメント
  movw    $0, %ax                 # Zero segments not ready for use
  movw    %ax, %fs                # -> FS: エクストラセグメント
  movw    %ax, %gs                # -> GS: エクストラセグメント

  # スタックポインタを設定しCのコードにとぶ。
  movl    $start, %esp
  call    bootmain # bootmain()関数へ

  # もしbootmain()関数からリターンした場合は(すべきでないが), 
  # Bochsで動作している場合にはブレイクポイントを起動後、ループに入る
  # Bochs = a highly portable open source IA-32 (x86) PC emulator written in C++.(http://bochs.sourceforge.net/)
  # http://bochs.sourceforge.net/doc/docbook/development/debugger-advanced.html
  movw    $0x8a00, %ax            # port 0x8a00: command register. 
  movw    %ax, %dx                
  outw    %ax, %dx                # write "0x8a00" -> port:0x8a00 = Used to enable the device.
  movw    $0x8ae0, %ax            
  outw    %ax, %dx                # write "0x8ae0" -> port:0x8a00 = Return to Debugger Prompt
spin: # 無限ループ
  jmp     spin

# 起動用のグローバルディスクリプタテーブル
.p2align 2                                # 4バイトアライメントを強制
gdt:
  SEG_NULLASM                             # NULLセグメント
  SEG_ASM(STA_X|STA_R, 0x0, 0xffffffff)   # コードセグメント(実行及び読み込み可能)。"0x0"から始まり、リミットは4GB(0xFFFFFFFF)(32bitモードで指定可能な最大のサイズ)
  SEG_ASM(STA_W, 0x0, 0xffffffff)         # データセグメント(書き込み可能)。"0x0"から始まり、リミットは4GB(0xFFFFFFFF)(32bitモードで指定可能な最大のサイズ)

# セグメントディスクリプタ
gdtdesc: # 
  .word   (gdtdesc - gdt - 1)             # グローバルディスクリプタテーブル(GDT)のサイズ - 1 (2バイト)
  .long   gdt                             # グローバルディスクリプタテーブル(gdt)のアドレス (4バイト)

