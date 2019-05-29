// 排他制御用のロック構造体
struct spinlock {
  uint locked;       // ロックされているか

  // For debugging:
  char *name;        // ロックの名前
  struct cpu *cpu;   // ロックを保持しているCPU
  uint pcs[10];      // ロックを取得している関数のコールスタック
};

