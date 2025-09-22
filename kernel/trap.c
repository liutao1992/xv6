#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[];

// in kernelvec.S, calls kerneltrap().
// 在 kernelvec.S 中调用 kerneltrap()
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
// 设置内核模式下的异常和陷阱处理
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from, and returns to, trampoline.S
// return value is user satp for trampoline.S to switch to.
//
// 处理来自用户空间的中断、异常或系统调用
// 从 trampoline.S 调用，并返回到 trampoline.S
// 返回值是用户 satp，供 trampoline.S 切换到
uint64
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  // 将中断和异常发送到 kerneltrap()，
  // 因为我们现在在内核中
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  // 保存用户程序计数器
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call
    // 系统调用

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    // sepc 指向 ecall 指令，
    // 但我们想返回到下一条指令
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    // 中断会改变 sepc、scause 和 sstatus，
    // 所以只有在我们处理完这些寄存器后才启用中断
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
    // 设备中断，正常处理
  } else if((r_scause() == 15 || r_scause() == 13) &&
            vmfault(p->pagetable, r_stval(), (r_scause() == 13)? 1 : 0) != 0) {
    // page fault on lazily-allocated page
    // 延迟分配页面上的页错误
  } else {
    printf("usertrap(): unexpected scause 0x%lx pid=%d\n", r_scause(), p->pid);
    printf("            sepc=0x%lx stval=0x%lx\n", r_sepc(), r_stval());
    setkilled(p);
  }

  if(killed(p))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  // 如果这是时钟中断，则让出 CPU
  if(which_dev == 2)
    yield();

  prepare_return();

  // the user page table to switch to, for trampoline.S
  // 要切换到的用户页表，供 trampoline.S 使用
  uint64 satp = MAKE_SATP(p->pagetable);

  // return to trampoline.S; satp value in a0.
  // 返回到 trampoline.S；satp 值在 a0 寄存器中
  return satp;
}

//
// set up trapframe and control registers for a return to user space
//
// 设置 trapframe 和控制寄存器以返回用户空间
//
void
prepare_return(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(). because a trap from kernel
  // code to usertrap would be a disaster, turn off interrupts.
  // 我们即将将陷阱目标从 kerneltrap() 切换到 usertrap()。
  // 因为从内核代码到 usertrap 的陷阱会是灾难性的，所以关闭中断
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  // 将系统调用、中断和异常发送到 trampoline.S 中的 uservec
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  // 设置 uservec 在进程下次陷入内核时需要的 trapframe 值
  p->trapframe->kernel_satp = r_satp();         // kernel page table 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack 进程的内核栈
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid() 用于 cpuid() 的硬件线程 ID

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  // 设置 trampoline.S 的 sret 指令用来进入用户空间的寄存器
  
  // set S Previous Privilege mode to User.
  // 将 S 前一特权模式设置为用户模式
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode 清除 SPP 为 0，用于用户模式
  x |= SSTATUS_SPIE; // enable interrupts in user mode 在用户模式下启用中断
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  // 将 S 异常程序计数器设置为保存的用户程序计数器
  w_sepc(p->trapframe->epc);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
// 来自内核代码的中断和异常通过 kernelvec 到达这里，
// 在当前内核栈上运行
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    // interrupt or trap from an unknown source
    // 来自未知源的中断或陷阱
    printf("scause=0x%lx sepc=0x%lx stval=0x%lx\n", scause, r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  // 如果这是时钟中断，则让出 CPU
  if(which_dev == 2 && myproc() != 0)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  // yield() 可能导致一些陷阱发生，
  // 所以恢复陷阱寄存器供 kernelvec.S 的 sepc 指令使用
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  if(cpuid() == 0){
    acquire(&tickslock);
    ticks++;
    wakeup(&ticks);
    release(&tickslock);
  }

  // ask for the next timer interrupt. this also clears
  // the interrupt request. 1000000 is about a tenth
  // of a second.
  // 请求下一次时钟中断。这也会清除中断请求。
  // 1000000 大约是十分之一秒
  w_stimecmp(r_time() + 1000000);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
// 检查是否是外部中断或软件中断，并处理它。
// 如果是时钟中断返回 2，
// 如果是其他设备返回 1，
// 如果未识别返回 0
int
devintr()
{
  uint64 scause = r_scause();

  if(scause == 0x8000000000000009L){
    // this is a supervisor external interrupt, via PLIC.
    // 这是通过 PLIC 的管理者外部中断

    // irq indicates which device interrupted.
    // irq 表示哪个设备产生了中断
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    // PLIC 允许每个设备一次最多产生一个中断；
    // 告诉 PLIC 设备现在可以再次中断
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // timer interrupt.
    // 时钟中断
    clockintr();
    return 2;
  } else {
    return 0;
  }
}

