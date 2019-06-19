// Long-term locks for processes
// プロセスのための長い期間のロック
struct sleeplock {
  uint locked;       // ロックが取得されているどうか
  struct spinlock lk; // このスリープロックを保護するためのスピンロック
  
  // デバッグ用:
  char *name;        // ロックの名前.
  int pid;           // ロックを保持しているプロセスのPID
};
