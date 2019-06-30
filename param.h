#define NPROC        64  // プロセスの最大数
#define KSTACKSIZE 4096  // 各プロセスのカーネルスタックのサイズ
#define NCPU          8  // CPU数の最大値
#define NOFILE       16  // プロセスがオープンできるファイル数
#define NFILE       100  // システムがオープンできるファイル数
#define NINODE       50  // アクティブ(キャッシュされている)な"inode"の最大数
#define NDEV         10  // "major device number"の最大数
#define ROOTDEV       1  // ルートディスクのファイルシステムのデバイス番号
#define MAXARG       32  // 指定可能な引数の最大数
#define MAXOPBLOCKS  10  // max # of blocks any FS op writes
#define LOGSIZE      (MAXOPBLOCKS*3)  // ディスク上にあるログの最大ブロック数
#define NBUF         (MAXOPBLOCKS*3)  // ディスクのブロックキャッシュの最大数
#define FSSIZE       1000  // 複数ブロック内にあるファイルシステムのサイズ

