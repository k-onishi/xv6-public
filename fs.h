// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO 1  // root i-number
#define BSIZE 512  // ブロックのサイズ

// ディスクのレイアウト:
// [ブートブロック | スーパーブロック | ログ | inodeブロック | フリービットマップ | データブロック]
// 
// mkfsコマンドはスーパーブロックを研鑽し、初期ファイルシステムを構築する。
// そのスーパーブロックはディスクのレイアウトを表現している
struct superblock {
  uint size;         // システムイメージのサイズ(ブロック数)
  uint nblocks;      // データブロックの数
  uint ninodes;      // inodeの数.
  uint nlog;         // ログブロックの数
  uint logstart;     // 初期ログブロックの数
  uint inodestart;   // 初期inodeブロックの数
  uint bmapstart;    // 初期フリーマップブロックの数
};

#define NDIRECT 12 // Not DIRECT
#define NINDIRECT (BSIZE / sizeof(uint)) // Not IN-DIRECT (16)
#define MAXFILE (NDIRECT + NINDIRECT) // 28

// ディスク上のinodeの構造体
struct dinode {
  short type;           // ファイル種別
  short major;          // デバイスのメジャー番号(T_DEV only)
  short minor;          // デバイスのマイナー番号(T_DEV only)
  short nlink;          // ファイルシステム内のinodeのリンク数
  uint size;            // ファイルサイズ(バイト)
  uint addrs[NDIRECT+1];   // データブロックアドレスのリスト
};

// ブロック毎のinodeの数(Inode Per Block)
#define IPB           (BSIZE / sizeof(struct dinode))

// ブロックが保持している"i"番のブロック
// inode番号をブロックが保持するinode数で除算し、inodeの開始位置のインデックスを加算する
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

