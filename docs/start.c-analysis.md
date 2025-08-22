# start.c 文件详细分析

## 概述

`start.c` 是 xv6-riscv 内核启动过程中的关键文件，负责从 Machine Mode 切换到 Supervisor Mode，并为内核的正式启动做准备。该文件在 `entry.S` 之后执行，是内核启动流程中的第二个阶段。

## 文件结构

### 头文件依赖

```c
#include "types.h"      // 基本数据类型定义
#include "param.h"      // 系统参数定义（如 NCPU）
#include "memlayout.h"  // 内存布局定义
#include "riscv.h"      // RISC-V 架构相关定义和 CSR 操作函数
#include "defs.h"       // 函数声明
```

### 全局变量

```c
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];
```

- **作用**: 为每个 CPU 核心提供启动阶段的栈空间
- **大小**: `4096 * NCPU` 字节（NCPU = 8，总共 32KB）
- **对齐**: 16 字节对齐，满足 RISC-V ABI 要求


## 主要函数分析

### start() 函数

`start()` 函数是内核从 Machine Mode 切换到 Supervisor Mode 的核心函数。

#### 1. 设置特权级模式

```c
// set M Previous Privilege mode to Supervisor, for mret.
unsigned long x = r_mstatus();
x &= ~MSTATUS_MPP_MASK;
x |= MSTATUS_MPP_S;
w_mstatus(x);
```

**功能说明**:
- 读取 `mstatus` 寄存器当前值
- 清除 MPP（Machine Previous Privilege）字段
- 设置 MPP 为 Supervisor Mode（`MSTATUS_MPP_S`）
- 写回 `mstatus` 寄存器

**技术细节**:
- `MSTATUS_MPP_MASK = (3L << 11)` - MPP 字段掩码
- `MSTATUS_MPP_S = (1L << 11)` - Supervisor Mode 标识
- 当执行 `mret` 指令时，CPU 将切换到 MPP 指定的特权级

#### 2. 设置异常返回地址

```c
// set M Exception Program Counter to main, for mret.
// requires gcc -mcmodel=medany
w_mepc((uint64)main);
```

**功能说明**:
- 将 `main` 函数地址写入 `mepc`（Machine Exception Program Counter）寄存器
- 当执行 `mret` 指令时，CPU 将跳转到 `mepc` 指定的地址

**编译要求**:
- 需要 `gcc -mcmodel=medany` 编译选项
- 确保代码可以在任意地址运行

#### 3. 禁用分页

```c
// disable paging for now.
w_satp(0);
```

**功能说明**:
- 将 `satp`（Supervisor Address Translation and Protection）寄存器设置为 0
- 禁用虚拟内存分页机制
- 此时使用物理地址直接访问内存

#### 4. 委托中断和异常

```c
// delegate all interrupts and exceptions to supervisor mode.
w_medeleg(0xffff);
w_mideleg(0xffff);
w_sie(r_sie() | SIE_SEIE | SIE_STIE);
```

**功能说明**:
- `w_medeleg(0xffff)`: 将所有异常委托给 Supervisor Mode 处理
- `w_mideleg(0xffff)`: 将所有中断委托给 Supervisor Mode 处理
- `w_sie(...)`: 启用 Supervisor Mode 的外部中断和定时器中断

**中断类型**:
- `SIE_SEIE = (1L << 9)` - Supervisor External Interrupt Enable
- `SIE_STIE = (1L << 5)` - Supervisor Timer Interrupt Enable

#### 5. 配置物理内存保护

```c
// configure Physical Memory Protection to give supervisor mode
// access to all of physical memory.
w_pmpaddr0(0x3fffffffffffffull);
w_pmpcfg0(0xf);
```

**功能说明**:
- 配置 PMP（Physical Memory Protection）允许 Supervisor Mode 访问所有物理内存
- `w_pmpaddr0(0x3fffffffffffffull)`: 设置 PMP 地址范围为最大值
- `w_pmpcfg0(0xf)`: 设置 PMP 配置为读写执行权限

#### 6. 初始化定时器

```c
// ask for clock interrupts.
timerinit();
```

**功能说明**:
- 调用 `timerinit()` 函数初始化定时器中断
- 为操作系统提供时钟中断支持

#### 7. 保存 Hart ID

```c
// keep each CPU's hartid in its tp register, for cpuid().
int id = r_mhartid();
w_tp(id);
```

**功能说明**:
- 读取当前 CPU 的 Hart ID
- 将 Hart ID 保存到 `tp`（Thread Pointer）寄存器
- 供后续的 `cpuid()` 函数使用

#### 8. 切换到 Supervisor Mode

```c
// switch to supervisor mode and jump to main().
asm volatile("mret");
```

**功能说明**:
- 执行 `mret`（Machine Return）指令
- CPU 切换到之前设置的 Supervisor Mode
- 跳转到 `mepc` 寄存器指定的 `main` 函数地址

### timerinit() 函数

`timerinit()` 函数负责初始化定时器中断系统。

#### 1. 启用 Supervisor 定时器中断

```c
// enable supervisor-mode timer interrupts.
w_mie(r_mie() | MIE_STIE);
```

**功能说明**:
- 在 `mie`（Machine Interrupt Enable）寄存器中启用 Supervisor 定时器中断
- `MIE_STIE = (1L << 5)` - Supervisor Timer Interrupt Enable

#### 2. 启用 SSTC 扩展

```c
// enable the sstc extension (i.e. stimecmp).
w_menvcfg(r_menvcfg() | (1L << 63));
```

**功能说明**:
- 启用 SSTC（Supervisor-mode Timer Compare）扩展
- 允许 Supervisor Mode 直接使用 `stimecmp` 寄存器
- 位 63 是 STCE（Supervisor Timer Compare Enable）位

#### 3. 允许 Supervisor 访问计数器

```c
// allow supervisor to use stimecmp and time.
w_mcounteren(r_mcounteren() | 2);
```

**功能说明**:
- 设置 `mcounteren` 寄存器允许 Supervisor Mode 访问时间相关寄存器
- 位 1 对应 `time` CSR 的访问权限

#### 4. 设置第一个定时器中断

```c
// ask for the very first timer interrupt.
w_stimecmp(r_time() + 1000000);
```

**功能说明**:
- 设置 `stimecmp`（Supervisor Timer Compare）寄存器
- 当前时间 + 1000000 个时钟周期后触发第一个定时器中断
- 为操作系统调度器提供时钟中断

## 启动流程总结

1. **entry.S** → 设置栈空间，跳转到 `start()`
2. **start()** → 配置 Machine Mode，切换到 Supervisor Mode
3. **main()** → 开始内核主要初始化流程

## 关键技术点

### RISC-V 特权级

- **Machine Mode (M)**: 最高特权级，可访问所有硬件资源
- **Supervisor Mode (S)**: 操作系统内核运行的特权级
- **User Mode (U)**: 用户程序运行的特权级

### CSR 寄存器

| 寄存器 | 全称 | 作用 |
|--------|------|------|
| `mstatus` | Machine Status | 机器状态寄存器 |
| `mepc` | Machine Exception PC | 机器异常程序计数器 |
| `medeleg` | Machine Exception Delegation | 机器异常委托 |
| `mideleg` | Machine Interrupt Delegation | 机器中断委托 |
| `mie` | Machine Interrupt Enable | 机器中断使能 |
| `sie` | Supervisor Interrupt Enable | 监管者中断使能 |
| `satp` | Supervisor Address Translation | 监管者地址转换 |
| `mhartid` | Machine Hart ID | 机器硬件线程 ID |
| `tp` | Thread Pointer | 线程指针 |

### 多核支持

- 每个 CPU 核心都会执行 `start()` 函数
- 通过 Hart ID 区分不同的 CPU 核心
- 每个核心使用独立的栈空间（`stack0` 数组）
- 所有核心最终都会跳转到 `main()` 函数

## 与其他文件的关系

- **entry.S**: 汇编启动代码，设置栈后跳转到 `start()`
- **main.c**: 内核主初始化函数，`start()` 最终跳转到此
- **riscv.h**: 提供 CSR 寄存器操作函数
- **param.h**: 定义系统参数如 `NCPU`
- **memlayout.h**: 定义内存布局常量

## 学习要点

1. **理解特权级切换**: 从 Machine Mode 到 Supervisor Mode 的过程
2. **掌握 CSR 操作**: RISC-V 控制状态寄存器的使用
3. **多核启动机制**: 如何在多核环境下安全启动
4. **中断系统初始化**: 定时器中断的配置过程
5. **内存保护**: PMP 的基本配置

`start.c` 文件虽然代码不多，但它是连接汇编启动代码和 C 语言内核代码的重要桥梁，展示了操作系统内核启动的核心技术。
