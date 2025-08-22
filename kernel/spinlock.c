// Mutual exclusion spin locks.
// 互斥自旋锁。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

// Initialize a spinlock
// 初始化自旋锁
void
initlock(struct spinlock *lk, char *name)
{
  lk->name = name;
  lk->locked = 0;
  lk->cpu = 0;
}

// Acquire the lock.
// 获取锁。
// Loops (spins) until the lock is acquired.
// 循环（自旋）直到获取到锁。
void
acquire(struct spinlock *lk)
{
  push_off(); // disable interrupts to avoid deadlock.
              // 禁用中断以避免死锁。
  if(holding(lk))
    panic("acquire");

  // On RISC-V, sync_lock_test_and_set turns into an atomic swap:
  // 在 RISC-V 上，sync_lock_test_and_set 转换为原子交换：
  //   a5 = 1
  //   s1 = &lk->locked
  //   amoswap.w.aq a5, a5, (s1)
  while(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    ;

  // Tell the C compiler and the processor to not move loads or stores
  // 告诉 C 编译器和处理器不要移动加载或存储指令
  // past this point, to ensure that the critical section's memory
  // 超过这一点，以确保临界区的内存
  // references happen strictly after the lock is acquired.
  // 引用严格在获取锁之后发生。
  // On RISC-V, this emits a fence instruction.
  // 在 RISC-V 上，这会发出一个 fence 指令。
  __sync_synchronize();

  // Record info about lock acquisition for holding() and debugging.
  // 记录锁获取信息，用于 holding() 和调试。
  lk->cpu = mycpu();
}

// Release the lock.
// 释放锁。
void
release(struct spinlock *lk)
{
  if(!holding(lk))
    panic("release");

  lk->cpu = 0;

  // Tell the C compiler and the CPU to not move loads or stores
  // 告诉 C 编译器和 CPU 不要移动加载或存储指令
  // past this point, to ensure that all the stores in the critical
  // 超过这一点，以确保临界区中的所有存储
  // section are visible to other CPUs before the lock is released,
  // 在释放锁之前对其他 CPU 可见，
  // and that loads in the critical section occur strictly before
  // 并且临界区中的加载严格在
  // the lock is released.
  // 锁被释放之前发生。
  // On RISC-V, this emits a fence instruction.
  // 在 RISC-V 上，这会发出一个 fence 指令。
  __sync_synchronize();

  // Release the lock, equivalent to lk->locked = 0.
  // 释放锁，等价于 lk->locked = 0。
  // This code doesn't use a C assignment, since the C standard
  // 这段代码不使用 C 赋值，因为 C 标准
  // implies that an assignment might be implemented with
  // 暗示赋值可能用
  // multiple store instructions.
  // 多个存储指令实现。
  // On RISC-V, sync_lock_release turns into an atomic swap:
  // 在 RISC-V 上，sync_lock_release 转换为原子交换：
  //   s1 = &lk->locked
  //   amoswap.w zero, zero, (s1)
  __sync_lock_release(&lk->locked);

  pop_off();
}

// Check whether this cpu is holding the lock.
// 检查当前 CPU 是否持有锁。
// Interrupts must be off.
// 中断必须关闭。
int
holding(struct spinlock *lk)
{
  int r;
  r = (lk->locked && lk->cpu == mycpu());
  return r;
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// push_off/pop_off 类似于 intr_off()/intr_on()，但它们是配对的：
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// 需要两个 pop_off() 来撤销两个 push_off()。另外，如果中断
// are initially off, then push_off, pop_off leaves them off.
// 最初是关闭的，那么 push_off, pop_off 会保持它们关闭。

void
push_off(void)
{
  int old = intr_get();

  // disable interrupts to prevent an involuntary context
  // 禁用中断以防止在使用 mycpu() 时发生
  // switch while using mycpu().
  // 非自愿的上下文切换。
  intr_off();

  if(mycpu()->noff == 0)
    mycpu()->intena = old;
  mycpu()->noff += 1;
}

void
pop_off(void)
{
  struct cpu *c = mycpu();
  if(intr_get())
    panic("pop_off - interruptible");
  if(c->noff < 1)
    panic("pop_off");
  c->noff -= 1;
  if(c->noff == 0 && c->intena)
    intr_on();
}
