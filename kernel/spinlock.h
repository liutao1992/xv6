#include "types.h"

// Mutual exclusion lock.
// 互斥锁。
struct spinlock {
  uint locked;       // Is the lock held?
                     // 锁是否被持有？

  // For debugging:
  // 用于调试：
  char *name;        // Name of lock.
                     // 锁的名称。
  struct cpu *cpu;   // The cpu holding the lock.
                     // 持有锁的 CPU。
};

