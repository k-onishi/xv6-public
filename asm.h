//
// assembler macros to create x86 segments
//

// NULLセグメントディスクリプタ(8バイト)
#define SEG_NULLASM                                             \
        .word 0, 0;                                             \
        .byte 0, 0, 0, 0

// "0xC0"の場合、制限の単位は4096バイトで32bitモードとなる
// セグメントディスクリプタを作成するマクロ
#define SEG_ASM(type,base,lim)                                  \
        .word (((lim) >> 12) & 0xffff), ((base) & 0xffff);      \
        .byte (((base) >> 16) & 0xff), (0x90 | (type)),         \
                (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#define STA_X     0x8       // 実行可能セグメント
#define STA_W     0x2       // 書き込み可能セグメント(実行不可)
#define STA_R     0x2       // 読み込み可能セグメント(実行可能)
