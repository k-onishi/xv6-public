// Memory layout

#define EXTMEM  0x100000            // Start of extended memory
#define PHYSTOP 0xE000000           // Top physical memory = 224MB
#define DEVSPACE 0xFE000000         // 他のデバイスがマッピングされるハイメモリ

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000         // カーネルの先頭仮想アドレス
#define KERNLINK (KERNBASE+EXTMEM)  // カーネルがリンクされているアドレス

#define V2P(a) (((uint) (a)) - KERNBASE) // 仮想アドレスを物理アドレスに変換
#define P2V(a) ((void *)(((char *) (a)) + KERNBASE)) // 物理アドレスを仮想アドレスに変換

#define V2P_WO(x) ((x) - KERNBASE)    // V2Pと同じ。型キャストがない
#define P2V_WO(x) ((x) + KERNBASE)    // P2Vと同じ。型キャストがない
