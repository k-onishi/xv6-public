# xv6のカーネルはこのファイルから実行を開始する。このファイルはカーネルとリンクされ
# main()などのカーネルシンボルの参照可能である。ブートブロック(bootasm.S及びbootmain.c)は
# 以下のentryにジャンプする。`
        
# マルチブートヘッダはGNU Grubのようなマルチブートローダのために存在する。
# http://www.gnu.org/software/grub/manual/multiboot/multiboot.html

# GRUB2を使用するに当たって、Linuxがkernelもしくはkernelmemfsを/bootに
# コピー若しくはこのメニューエントリを追加することで保存したファイルから起動することができる。
# menuentry "xv6" {
# 	insmod ext2
# 	set root='(hd0,msdos1)'
# 	set kernel='/boot/kernel'
# 	echo "Loading ${kernel}..."
# 	multiboot ${kernel} ${kernel}
# 	boot
# }

#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"

# マルチヘッダはマルチブートローダのデータである
.p2align 2
.text
.globl multiboot_header
multiboot_header:
  #define magic 0x1badb002
  #define flags 0
  .long magic
  .long flags
  .long (-magic-flags)

# 慣習として、_startシンボルがELFのエントリポイントを指定する。
# 依然として仮想メモリをセットアップしていないので、エントリポイントは物理アドレス
# のentryとなる。
.globl _start
_start = V2P_WO(entry)

# ページングを切った状態での起動プロセッサ上でのxv6の起動処理
.globl entry
entry:
  # ページサイズ拡張(4MByte)を有効化
  # http://caspar.hazymoon.jp/OpenBSD/annex/intel_arc.html
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax # bit5をセット
  movl    %eax, %cr4
  # ページディレクトリを設定
  movl    $(V2P_WO(entrypgdir)), %eax
  movl    %eax, %cr3
  # ページング及び書き込みに対するプロテクションを有効化
  movl    %cr0, %eax
  orl     $(CR0_PG|CR0_WP), %eax # WRITE_PROTECTION | PAGING
  movl    %eax, %cr0

  # スタックポインタをセットアップ(以下の".comm stack, KSTACKSIZE"で確保したスペースをスタックに)
  movl $(stack + KSTACKSIZE), %esp

  # main()に飛び、高位アドレスで実行するよう遷移する。
  # 間接呼び出しになっているのは、直接的なジャンプの場合アセンブラが
  # プログラムカウンタに対して相対的アドレッシングを用いた命令を生成するためである
  mov $main, %eax
  jmp *%eax

# スタック用のスペース
.comm stack, KSTACKSIZE # 4096 bytes
