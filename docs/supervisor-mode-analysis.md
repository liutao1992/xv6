# Supervisor Mode 详细介绍

## 概述

**Supervisor Mode**（监管者模式）是 RISC-V 架构中的一个重要特权级，位于 Machine Mode 和 User Mode 之间，专门为操作系统内核设计。

## 特权级层次结构

RISC-V 架构定义了三个主要特权级：

```
Machine Mode (M-Mode)     ← 最高特权级
    ↓
Supervisor Mode (S-Mode)  ← 操作系统内核
    ↓
User Mode (U-Mode)        ← 用户程序
```

### 特权级编码

| 特权级 | 编码 | 名称 | 用途 |
|--------|------|------|------|
| 11 | 3 | Machine Mode | 硬件抽象层、固件 |
| 01 | 1 | Supervisor Mode | 操作系统内核 |
| 00 | 0 | User Mode | 用户应用程序 |

## Supervisor Mode 的特点

### 1. 权限范围

- ✅ 可以访问大部分系统资源
- ✅ 可以管理虚拟内存（页表）
- ✅ 可以处理大部分中断和异常
- ❌ 无法直接访问某些硬件资源（需要通过 Machine Mode）
- ❌ 无法修改 Machine Mode 的 CSR 寄存器

### 2. 主要职责

- **内存管理**：虚拟内存、页表管理
- **进程调度**：进程创建、切换、销毁
- **中断处理**：处理委托的中断和异常
- **系统调用**：为用户程序提供系统服务
- **设备驱动**：管理大部分 I/O 设备

## 关键 CSR 寄存器

### 状态寄存器

#### sstatus - Supervisor Status Register

```c
static inline uint64 r_sstatus() {
    uint64 x;
    asm volatile("csrr %0, sstatus" : "=r" (x));
    return x;
}

static inline void w_sstatus(uint64 x) {
    asm volatile("csrw sstatus, %0" : : "r" (x));
}
```

**重要字段**：
- **SPP (bit 8)**：Previous Privilege（之前的特权级）
  - 1 = Supervisor Mode
  - 0 = User Mode
- **SPIE (bit 5)**：Previous Interrupt Enable（之前的中断使能状态）
- **SIE (bit 1)**：Interrupt Enable（当前中断使能）

### 中断相关寄存器

#### sie - Supervisor Interrupt Enable

```c
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software

static inline uint64 r_sie() {
    uint64 x;
    asm volatile("csrr %0, sie" : "=r" (x));
    return x;
}
```

#### sip - Supervisor Interrupt Pending

```c
static inline uint64 r_sip() {
    uint64 x;
    asm volatile("csrr %0, sip" : "=r" (x));
    return x;
}
```

#### stvec - Supervisor Trap Vector

```c
static inline void w_stvec(uint64 x) {
    asm volatile("csrw stvec, %0" : : "r" (x));
}
```

**模式**：
- **Direct Mode (0)**：所有异常跳转到 BASE 地址
- **Vectored Mode (1)**：中断跳转到 BASE + 4×cause

### 异常处理寄存器

#### sepc - Supervisor Exception Program Counter

```c
static inline uint64 r_sepc() {
    uint64 x;
    asm volatile("csrr %0, sepc" : "=r" (x));
    return x;
}

static inline void w_sepc(uint64 x) {
    asm volatile("csrw sepc, %0" : : "r" (x));
}
```

#### scause - Supervisor Cause Register

```c
static inline uint64 r_scause() {
    uint64 x;
    asm volatile("csrr %0, scause" : "=r" (x));
    return x;
}
```

**异常代码**：
- 0: Instruction address misaligned
- 1: Instruction access fault
- 2: Illegal instruction
- 8: Environment call from U-mode
- 12: Instruction page fault
- 13: Load page fault
- 15: Store/AMO page fault

#### stval - Supervisor Trap Value

```c
static inline uint64 r_stval() {
    uint64 x;
    asm volatile("csrr %0, stval" : "=r" (x));
    return x;
}
```

### 内存管理寄存器

#### satp - Supervisor Address Translation and Protection

```c
#define SATP_SV39 (8L << 60)
#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

static inline void w_satp(uint64 x) {
    asm volatile("csrw satp, %0" : : "r" (x));
}

static inline uint64 r_satp() {
    uint64 x;
    asm volatile("csrr %0, satp" : "=r" (x));
    return x;
}
```

**字段**：
- **MODE (bits 63-60)**：地址转换模式
  - 0: Bare（无地址转换）
  - 8: Sv39（39位虚拟地址）
  - 9: Sv48（48位虚拟地址）
- **ASID (bits 59-44)**：地址空间标识符
- **PPN (bits 43-0)**：页表物理页号

## 在 xv6-riscv 中的应用

### 1. 启动过程中的特权级切换

在 `kernel/start.c` 中：

```c
void start() {
    // 设置返回到 Supervisor Mode
    unsigned long x = r_mstatus();
    x &= ~MSTATUS_MPP_MASK;
    x |= MSTATUS_MPP_S;          // 设置为 Supervisor Mode
    w_mstatus(x);
    
    // 设置返回地址为 main 函数
    w_mepc((uint64)main);
    
    // 委托中断和异常给 Supervisor Mode
    w_medeleg(0xffff);  // 异常委托
    w_mideleg(0xffff);  // 中断委托
    
    // 启用 Supervisor 中断
    w_sie(r_sie() | SIE_SEIE | SIE_STIE);
    
    // 执行 mret 切换到 Supervisor Mode
    asm volatile("mret");
}
```

### 2. 虚拟内存管理

在 `kernel/vm.c` 中：

```c
void kvminithart() {
    // 启用 Sv39 分页模式
    w_satp(MAKE_SATP(kernel_pagetable));
    sfence_vma();  // 刷新 TLB
}

void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm) {
    if(mappages(kpgtbl, va, sz, pa, perm) != 0)
        panic("kvmmap");
}
```

### 3. 中断处理设置

在 `kernel/trap.c` 中：

```c
void trapinit(void) {
    initlock(&tickslock, "time");
}

void trapinithart(void) {
    w_stvec((uint64)kernelvec);  // 设置内核陷阱向量
}
```

## 与其他模式的交互

### Machine Mode → Supervisor Mode

**切换方式**：
```assembly
# 在 Machine Mode 中
mret    # Machine Return，根据 mstatus.MPP 切换特权级
```

**切换条件**：
- `mstatus.MPP` 设置为 Supervisor Mode (01)
- `mepc` 设置为目标地址
- 执行 `mret` 指令

### Supervisor Mode → User Mode

**切换方式**：
```assembly
# 在 Supervisor Mode 中
sret    # Supervisor Return，根据 sstatus.SPP 切换特权级
```

**在 xv6 中的实现**：
```c
// kernel/proc.c - 创建用户进程
void userinit(void) {
    struct proc *p = allocproc();
    initproc = p;
    
    // 设置用户页表
    p->pagetable = proc_pagetable(p);
    
    // 加载用户程序
    uvmfirst(p->pagetable, initcode, sizeof(initcode));
    
    // 设置返回到用户模式
    p->trapframe->epc = 0;      // 用户程序入口
    p->trapframe->sp = PGSIZE;  // 用户栈指针
    
    p->state = RUNNABLE;
}
```

### User Mode → Supervisor Mode

**触发方式**：
1. **系统调用**（`ecall` 指令）
2. **异常**（页错误、非法指令等）
3. **中断**（定时器、外部设备等）

**处理流程**：
```c
// kernel/trap.c
void usertrap(void) {
    if(r_sstatus() & SSTATUS_SPP)
        panic("usertrap: not from user mode");
    
    // 设置内核陷阱向量
    w_stvec((uint64)kernelvec);
    
    struct proc *p = myproc();
    p->trapframe->epc = r_sepc();
    
    if(r_scause() == 8) {
        // 系统调用
        if(killed(p))
            exit(-1);
        
        p->trapframe->epc += 4;  // 跳过 ecall 指令
        intr_on();
        syscall();
    } else if((which_dev = devintr()) != 0) {
        // 设备中断
    } else {
        // 异常处理
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        setkilled(p);
    }
    
    usertrapret();
}
```

## 内存保护机制

### 1. 虚拟内存

**Sv39 地址转换**：
- 39位虚拟地址空间（512GB）
- 三级页表结构
- 4KB 页面大小

```
虚拟地址格式 (39位):
[38:30] [29:21] [20:12] [11:0]
  VPN2    VPN1    VPN0   offset
```

**页表项格式**：
```c
#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1) // readable
#define PTE_W (1L << 2) // writable
#define PTE_X (1L << 3) // executable
#define PTE_U (1L << 4) // user can access
#define PTE_G (1L << 5) // global
#define PTE_A (1L << 6) // accessed
#define PTE_D (1L << 7) // dirty
```

### 2. 地址空间隔离

**内核空间**：
```c
// kernel/memlayout.h
#define KERNBASE 0x80000000L
#define PHYSTOP (KERNBASE + 128*1024*1024)

// 内核虚拟地址映射
#define TRAMPOLINE (MAXVA - PGSIZE)
#define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)
#define TRAPFRAME (TRAMPOLINE - PGSIZE)
```

**用户空间**：
```c
// 用户虚拟地址布局
// 0x0000000000000000 - 程序代码和数据
// ...                - 堆空间
// TRAPFRAME          - 陷阱帧
// TRAMPOLINE         - 陷阱跳板
```

## 中断处理流程

### 1. 中断发生时的硬件行为

```
1. 保存当前 PC 到 sepc
2. 设置 scause（中断/异常原因）
3. 设置 stval（相关地址或值）
4. 设置 sstatus.SPIE = sstatus.SIE
5. 清除 sstatus.SIE（禁用中断）
6. 设置 sstatus.SPP = 当前特权级
7. 切换到 Supervisor Mode
8. 跳转到 stvec 指定的地址
```

### 2. 软件中断处理

```c
// kernel/kernelvec.S - 内核中断入口
kernelvec:
    // 保存寄存器
    addi sp, sp, -256
    sd ra, 0(sp)
    sd sp, 8(sp)
    // ... 保存所有寄存器
    
    // 调用 C 语言中断处理函数
    call kerneltrap
    
    // 恢复寄存器
    ld ra, 0(sp)
    ld sp, 8(sp)
    // ... 恢复所有寄存器
    addi sp, sp, 256
    
    sret  // 返回到中断前的位置
```

### 3. 中断返回

```assembly
sret    # Supervisor Return
```

**硬件行为**：
1. 恢复 PC = sepc
2. 恢复 sstatus.SIE = sstatus.SPIE
3. 切换特权级到 sstatus.SPP
4. 清除 sstatus.SPP = 0

## 系统调用机制

### 1. 用户程序发起系统调用

```c
// user/usys.pl 生成的系统调用存根
.global fork
fork:
 li a7, SYS_fork    // 系统调用号
 ecall              // 环境调用
 ret
```

### 2. 内核处理系统调用

```c
// kernel/syscall.c
void syscall(void) {
    int num;
    struct proc *p = myproc();
    
    num = p->trapframe->a7;  // 获取系统调用号
    if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
        p->trapframe->a0 = syscalls[num]();  // 调用对应函数
    } else {
        printf("%d %s: unknown sys call %d\n",
                p->pid, p->name, num);
        p->trapframe->a0 = -1;
    }
}
```

### 3. 系统调用表

```c
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
};
```

## 设计优势

### 1. 安全隔离
- **特权级隔离**：用户程序无法直接访问内核资源
- **地址空间隔离**：每个进程有独立的虚拟地址空间
- **权限控制**：通过页表权限位控制内存访问

### 2. 灵活性
- **虚拟内存**：支持复杂的内存管理策略
- **中断委托**：Machine Mode 可以选择性委托中断
- **可扩展性**：支持多种地址转换模式

### 3. 性能
- **硬件支持**：快速的上下文切换
- **TLB 缓存**：地址转换缓存提高性能
- **直接跳转**：中断向量表支持直接跳转

### 4. 可移植性
- **标准化**：RISC-V 标准定义的特权级架构
- **一致性**：所有 RISC-V 实现都支持相同的接口
- **简洁性**：相比 x86 更简洁的设计

## 与 x86 架构对比

| 特性 | RISC-V Supervisor Mode | x86 Ring 0 |
|------|------------------------|------------|
| **特权级数量** | 3级（M/S/U） | 4级（Ring 0-3，实际常用2级） |
| **地址转换** | satp 寄存器 | CR3 寄存器 |
| **中断处理** | stvec 向量表 | IDT 中断描述符表 |
| **系统调用** | ecall 指令 | syscall/sysenter 指令 |
| **页表格式** | Sv39/Sv48 | PAE/IA-32e |
| **设计哲学** | 简洁、模块化 | 复杂、向后兼容 |

## 学习要点

### 1. 核心概念
- **理解特权级概念**：掌握三级特权模式的设计思想
- **掌握 CSR 操作**：熟悉 Supervisor Mode 相关的控制寄存器
- **内存管理**：理解虚拟内存和页表机制
- **中断处理**：掌握中断/异常的处理流程
- **系统调用**：理解用户态和内核态的切换机制

### 2. 实践技能
- **调试技巧**：使用 GDB 调试内核代码
- **性能分析**：理解不同操作的性能开销
- **安全考虑**：理解潜在的安全漏洞和防护措施

### 3. 扩展学习
- **虚拟化**：理解 Hypervisor 扩展
- **多核同步**：理解 SMP 系统中的同步机制
- **实时系统**：理解实时操作系统的特殊需求

## 总结

Supervisor Mode 是现代操作系统的核心运行环境，它提供了：

- **安全的执行环境**：通过特权级和虚拟内存保护系统资源
- **高效的资源管理**：支持复杂的内存管理和进程调度
- **灵活的中断处理**：支持多种中断和异常处理机制
- **标准化的接口**：为操作系统提供一致的硬件抽象

理解 Supervisor Mode 是深入学习操作系统内核、系统编程和计算机体系结构的关键基础。通过 xv6-riscv 的实际代码，我们可以看到这些概念在真实操作系统中的具体实现和应用。