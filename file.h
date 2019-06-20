struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};

// inodeのメモリ上のコピー
struct inode {
  uint dev;           // デバイス番号
  uint inum;          // inode番号
  int ref;            // 参照カウンタ
  struct sleeplock lock; // ここから下の全てのメンバを保護するためのロック
  int valid;          // inodeがディスクから読み込まれているか

  short type;         // ディスクinodeのコピー
  short major;        // メジャー番号
  short minor;        // マイナー番号
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
};

// メジャーデバイス番号とそれに対応する関数がマッピングされたテーブル
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

// メジャーデバイス(計10個)のウチのコンソールデバイスに対応する番号
#define CONSOLE 1
