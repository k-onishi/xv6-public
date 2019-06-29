#include "asm.h"
#include "memlayout.h"
#include "mmu.h"
	
# ブートCPU以外の各CPU(AP)はブートCPU(BP)からのIPI(Inter-Processor Interrupt)の"STARTUP"
# の応答によって起動される。"Multi-Processor Sepcification"によるとAPはリアルモードで
# IPIで送られてくる8-bitの値で指定された0x**00:0000(CS:IP)から起動する。
# よってこのコードは4096バイトの境界上で起動する。

# ココのコードがDSに0をセットするため、下位16バイトのアドレスに収まる。
# Startothers(main.c)は”STARTUP"を1つずつ送信する。
# このコードを0x7000にコピーする。プロセッサ毎に新たに割り当てられた
# スタックのアドレス(start-4)、mpenterにジャンプするために割り当てたられたアドレス(start-8)
# そしてentrypgdirの物理アドレス(start-12)が配置される。

# このコードはbootasm.Sとentry.Sの要素を組み合わせたものである。

.code16           
.globl start
start:
  cli            

  # Zero data segment registers DS, ES, and SS.
  xorw    %ax,%ax
  movw    %ax,%ds
  movw    %ax,%es
  movw    %ax,%ss

  # Switch from real to protected mode.  Use a bootstrap GDT that makes
  # virtual addresses map directly to physical addresses so that the
  # effective memory map doesn't change during the transition.
  lgdt    gdtdesc
  movl    %cr0, %eax
  orl     $CR0_PE, %eax
  movl    %eax, %cr0

  # Complete the transition to 32-bit protected mode by using a long jmp
  # to reload %cs and %eip.  The segment descriptors are set up with no
  # translation, so that the mapping is still the identity mapping.
  ljmpl    $(SEG_KCODE<<3), $(start32)

//PAGEBREAK!
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

  # Turn on page size extension for 4Mbyte pages
  movl    %cr4, %eax
  orl     $(CR4_PSE), %eax
  movl    %eax, %cr4
  # Use entrypgdir as our initial page table
  movl    (start-12), %eax
  movl    %eax, %cr3
  # Turn on paging.
  movl    %cr0, %eax
  orl     $(CR0_PE|CR0_PG|CR0_WP), %eax
  movl    %eax, %cr0

  # Switch to the stack allocated by startothers()
  movl    (start-4), %esp
  # Call mpenter()
  call	 *(start-8)

  movw    $0x8a00, %ax
  movw    %ax, %dx
  outw    %ax, %dx
  movw    $0x8ae0, %ax
  outw    %ax, %dx
spin:
  jmp     spin

.p2align 2
gdt:
  SEG_NULLASM
  SEG_ASM(STA_X|STA_R, 0, 0xffffffff)
  SEG_ASM(STA_W, 0, 0xffffffff)


gdtdesc:
  .word   (gdtdesc - gdt - 1)
  .long   gdt

