# 使用 GDB 调试 XV6 代码完整教程

## 概述

GDB (GNU Debugger) 是调试 XV6 操作系统的强大工具。本教程将详细介绍如何设置和使用 GDB 来调试 XV6 代码，包括内核启动、系统调用、进程管理等各个方面。

## 环境准备

### 1. 安装必要工具

确保你的系统已安装以下工具：

```bash
# Ubuntu/Debian
sudo apt-get install gdb-multiarch qemu-system-misc

# macOS (使用 Homebrew)
brew install riscv64-elf-gdb qemu

# 或者使用预编译的 RISC-V 工具链
# 从 https://github.com/riscv/riscv-gnu-toolchain 下载
```

### 2. 验证工具版本

```bash
# 检查 GDB 版本 (需要支持 RISC-V)
riscv64-unknown-elf-gdb --version

# 检查 QEMU 版本
qemu-system-riscv64 --version
```

### 3. XV6 编译配置

确保 XV6 编译时包含调试信息：

```bash
# 查看 Makefile 中的编译选项
grep CFLAGS Makefile
# 应该包含 -g 选项用于生成调试信息

# 编译 XV6
make clean
make qemu
```

## GDB 基础设置

### 1. 创建 GDB 配置文件

创建 `.gdbinit` 文件来自动化 GDB 设置：

```bash
# 在 XV6 项目根目录创建 .gdbinit
cat > .gdbinit << 'EOF'
# 连接到 QEMU
define qemu
    target remote localhost:26000
end

# 设置架构
set architecture riscv:rv64

# 显示汇编代码
set disassemble-next-line on

# 显示更多上下文
set listsize 20

# 美化输出
set print pretty on
set print array on

# 自动加载符号
symbol-file kernel/kernel

# 常用断点设置
define bp-main
    break main
end

define bp-syscall
    break syscall
end

define bp-usertrap
    break usertrap
end

define bp-kerneltrap
    break kerneltrap
end

# 显示当前 CPU 信息
define cpu-info
    printf "Current CPU: %d\n", (int)cpuid()
    printf "Current process: %d (%s)\n", myproc()->pid, myproc()->name
end

# 显示栈回溯
define bt-all
    thread apply all bt
end

# 显示寄存器
define regs
    info registers
end

# 显示 RISC-V CSR 寄存器
define csr
    printf "sstatus: 0x%lx\n", $sstatus
    printf "sepc:    0x%lx\n", $sepc
    printf "scause:  0x%lx\n", $scause
    printf "stval:   0x%lx\n", $stval
end

echo .gdbinit loaded successfully\n
EOF
```

### 2. 启动调试会话

#### 方法一：分步启动
```bash
# 终端1: 启动 QEMU (调试模式)
make qemu-gdb

# 终端2: 启动 GDB
riscv64-unknown-elf-gdb
(gdb) qemu
(gdb) continue
```

#### 方法二：一键启动
```bash
# 直接启动并连接
make qemu-gdb &
riscv64-unknown-elf-gdb -ex "target remote localhost:26000" -ex "continue"
```

## GDB 基本调试命令

### 1. 连接和控制命令

```bash
# 连接到 QEMU
(gdb) target remote localhost:26000

# 继续执行
(gdb) continue
(gdb) c

# 单步执行
(gdb) step      # 进入函数
(gdb) s
(gdb) next      # 跳过函数
(gdb) n

# 执行一条汇编指令
(gdb) stepi
(gdb) si
(gdb) nexti
(gdb) ni

# 暂停执行
Ctrl+C

# 重启程序
(gdb) monitor system_reset
```

### 2. 断点管理

```bash
# 设置断点
(gdb) break main                    # 在 main 函数设置断点
(gdb) break kernel/proc.c:100       # 在指定文件行号设置断点
(gdb) break usertrap                # 在函数设置断点
(gdb) break *0x80000000             # 在地址设置断点

# 条件断点
(gdb) break syscall if $a7 == 1     # 当 a7 寄存器为 1 时断点
(gdb) break allocproc if pid > 5    # 当变量满足条件时断点

# 临时断点
(gdb) tbreak main                   # 只触发一次的断点

# 查看断点
(gdb) info breakpoints
(gdb) info b

# 禁用/启用断点
(gdb) disable 1                     # 禁用断点1
(gdb) enable 1                      # 启用断点1

# 删除断点
(gdb) delete 1                      # 删除断点1
(gdb) clear main                    # 删除 main 函数的断点
```

### 3. 查看代码和内存

```bash
# 查看源码
(gdb) list                          # 显示当前位置代码
(gdb) list main                     # 显示 main 函数代码
(gdb) list kernel/proc.c:100        # 显示指定文件行号

# 反汇编
(gdb) disassemble                   # 反汇编当前函数
(gdb) disassemble main              # 反汇编指定函数
(gdb) disassemble $pc,$pc+40        # 反汇编指定范围

# 查看内存
(gdb) x/10i $pc                     # 查看程序计数器处的10条指令
(gdb) x/10x 0x80000000              # 查看地址处的16进制内容
(gdb) x/10s 0x80001000              # 查看字符串
(gdb) x/10w $sp                     # 查看栈内容 (word)

# 查看变量
(gdb) print variable_name           # 打印变量值
(gdb) print *ptr                    # 打印指针指向的内容
(gdb) print/x variable              # 以16进制打印
(gdb) print/t variable              # 以二进制打印

# 查看结构体
(gdb) print *myproc()               # 打印当前进程结构体
(gdb) print proc[0]                 # 打印进程表第一个进程
```

### 4. 寄存器操作

```bash
# 查看所有寄存器
(gdb) info registers
(gdb) info reg

# 查看特定寄存器
(gdb) info reg sp                   # 查看栈指针
(gdb) info reg a0                   # 查看参数寄存器
(gdb) print $pc                     # 打印程序计数器

# 查看 RISC-V CSR 寄存器
(gdb) info reg sstatus
(gdb) info reg sepc
(gdb) info reg scause

# 修改寄存器值
(gdb) set $a0 = 42                  # 设置寄存器值
(gdb) set $pc = 0x80000000          # 修改程序计数器
```

### 5. 栈和调用跟踪

```bash
# 显示调用栈
(gdb) backtrace                     # 显示完整调用栈
(gdb) bt                            # 简写
(gdb) bt 10                         # 显示最近10层调用

# 切换栈帧
(gdb) frame 0                       # 切换到第0帧
(gdb) up                            # 向上一帧
(gdb) down                          # 向下一帧

# 查看当前帧信息
(gdb) info frame                    # 显示当前帧详细信息
(gdb) info locals                   # 显示局部变量
(gdb) info args                     # 显示函数参数
```

## 调试 XV6 内核启动

### 1. 调试启动过程

```bash
# 启动 GDB 并连接
riscv64-unknown-elf-gdb kernel/kernel
(gdb) target remote localhost:26000

# 在入口点设置断点
(gdb) break _entry
(gdb) continue

# 单步跟踪启动过程
(gdb) si                            # 执行汇编指令
(gdb) info reg sp                   # 查看栈指针设置
(gdb) x/10i $pc                     # 查看即将执行的指令

# 跟踪到 start() 函数
(gdb) break start
(gdb) continue

# 查看 CPU 初始化
(gdb) step
(gdb) print cpuid()                 # 查看当前 CPU ID
```

### 2. 调试多核启动

```bash
# 观察多个 CPU 的启动
(gdb) break _entry
(gdb) continue

# 第一次命中断点 (CPU 0)
(gdb) info reg mhartid              # 查看硬件线程 ID
(gdb) continue

# 第二次命中断点 (CPU 1)
(gdb) info reg mhartid
(gdb) print $sp                     # 比较不同 CPU 的栈指针
```

## 调试系统调用

### 1. 跟踪系统调用流程

```bash
# 在系统调用入口设置断点
(gdb) break usertrap
(gdb) break syscall

# 运行到系统调用
(gdb) continue

# 在 usertrap 中查看陷阱原因
(gdb) print/x $scause               # 应该是 8 (系统调用)
(gdb) print $a7                     # 查看系统调用号

# 单步进入 syscall 处理
(gdb) step
(gdb) print num                     # 查看系统调用号
```

### 2. 特定系统调用调试

```bash
# 调试 fork 系统调用
(gdb) break sys_fork
(gdb) continue
(gdb) step
(gdb) print *myproc()               # 查看当前进程

# 调试 exec 系统调用
(gdb) break sys_exec
(gdb) continue
(gdb) print path                    # 查看要执行的程序路径
```

## 调试进程管理

### 1. 进程创建和切换

```bash
# 调试进程分配
(gdb) break allocproc
(gdb) continue
(gdb) step
(gdb) print p->pid                  # 查看分配的进程 ID

# 调试进程调度
(gdb) break sched
(gdb) continue
(gdb) print *myproc()               # 查看当前进程
(gdb) step
(gdb) print *myproc()               # 切换后的进程
```

### 2. 查看进程表

```bash
# 查看所有进程
(gdb) print proc[0]
(gdb) print proc[1]
(gdb) print proc[2]

# 使用循环查看所有进程
(gdb) set $i = 0
(gdb) while $i < 64
> if proc[$i].state != 0
>   printf "PID: %d, Name: %s, State: %d\n", proc[$i].pid, proc[$i].name, proc[$i].state
> end
> set $i = $i + 1
> end
```

## 调试内存管理

### 1. 页表调试

```bash
# 查看页表结构
(gdb) break kvminit
(gdb) continue
(gdb) print/x kernel_pagetable      # 查看内核页表

# 调试用户页表
(gdb) break uvmcreate
(gdb) continue
(gdb) step
(gdb) print/x $result               # 查看创建的页表
```

### 2. 内存分配调试

```bash
# 调试 kalloc
(gdb) break kalloc
(gdb) continue
(gdb) step
(gdb) print/x $result               # 查看分配的物理地址

# 查看空闲页面链表
(gdb) print freelist
(gdb) print freelist->next
```

## 高级调试技巧

### 1. 条件断点和监视点

```bash
# 设置条件断点
(gdb) break fork if pid > 5         # 当 PID 大于 5 时停止
(gdb) break kalloc if size > 4096   # 当分配大于 4KB 时停止

# 设置监视点 (变量改变时停止)
(gdb) watch global_variable
(gdb) watch *0x80001000             # 监视内存地址

# 读写监视点
(gdb) rwatch variable               # 读取时停止
(gdb) awatch variable               # 读写时都停止
```

### 2. 自定义调试函数

在 `.gdbinit` 中添加自定义函数：

```bash
# 显示进程信息
define show-proc
    if $argc == 1
        set $p = &proc[$arg0]
        printf "PID: %d\n", $p->pid
        printf "Name: %s\n", $p->name
        printf "State: %d\n", $p->state
        printf "Parent: %d\n", $p->parent ? $p->parent->pid : -1
    else
        printf "Usage: show-proc <pid>\n"
    end
end

# 显示调用栈（带符号）
define bt-verbose
    set $i = 0
    while $i < 10
        frame $i
        info line
        set $i = $i + 1
    end
end

# 显示内存映射
define show-vm
    printf "Kernel page table: 0x%lx\n", kernel_pagetable
    if $argc == 1
        set $p = &proc[$arg0]
        printf "Process %d page table: 0x%lx\n", $p->pid, $p->pagetable
    end
end
```

### 3. 脚本化调试

创建 GDB 脚本文件 `debug_script.gdb`：

```bash
# debug_script.gdb
target remote localhost:26000

# 设置多个断点
break main
break usertrap
break syscall

# 运行到 main
continue

# 自动执行一系列命令
commands 1
    printf "Reached main function\n"
    info reg
    continue
end

# 开始调试
continue
```

使用脚本：
```bash
riscv64-unknown-elf-gdb -x debug_script.gdb kernel/kernel
```

## 常见调试场景

### 1. 调试内核崩溃

```bash
# 设置在 panic 函数的断点
(gdb) break panic
(gdb) continue

# 查看崩溃信息
(gdb) bt                            # 调用栈
(gdb) info reg                      # 寄存器状态
(gdb) x/10i $pc                     # 崩溃位置的指令

# 查看相关变量
(gdb) print error_message
(gdb) print *current_process
```

### 2. 调试死锁

```bash
# 在锁操作处设置断点
(gdb) break acquire
(gdb) break release

# 查看锁的状态
(gdb) print lock->locked
(gdb) print lock->name
(gdb) print lock->cpu

# 检查所有 CPU 的状态
(gdb) thread apply all bt
```

### 3. 调试性能问题

```bash
# 使用采样方式
(gdb) break timer_interrupt
(gdb) commands
>     bt 5
>     continue
> end

# 定期中断查看程序状态
(gdb) break clockintr
(gdb) commands
>     printf "PC: 0x%lx\n", $pc
>     continue
> end
```

## 调试技巧和最佳实践

### 1. 符号和调试信息

```bash
# 加载额外的符号信息
(gdb) symbol-file user/_init
(gdb) add-symbol-file user/_sh 0x1000

# 查看符号信息
(gdb) info functions               # 列出所有函数
(gdb) info variables              # 列出全局变量
(gdb) info types                  # 列出类型信息
```

### 2. 远程调试优化

```bash
# 设置更快的连接
(gdb) set remotetimeout 30
(gdb) set debug remote 1          # 开启调试输出

# 批量操作
(gdb) set confirm off             # 关闭确认提示
(gdb) set pagination off         # 关闭分页
```

### 3. 日志和输出

```bash
# 设置日志记录
(gdb) set logging on
(gdb) set logging file debug.log

# 自定义输出格式
(gdb) set print address on
(gdb) set print symbol-filename on
```

## 故障排除

### 1. 常见问题

**问题**: 无法连接到 QEMU
```bash
# 解决方案
1. 确保 QEMU 已启动并监听端口 26000
2. 检查防火墙设置
3. 尝试不同的端口号

# 检查 QEMU 监听端口
netstat -tln | grep 26000
```

**问题**: 符号信息缺失
```bash
# 解决方案
1. 确保编译时使用了 -g 选项
2. 检查 kernel/kernel 文件是否存在
3. 重新编译项目

make clean
make DEBUG=1
```

**问题**: 断点不生效
```bash
# 解决方案
1. 确保地址正确
2. 检查是否在正确的文件中设置断点
3. 使用地址断点代替符号断点

(gdb) info line main              # 查看函数地址
(gdb) break *0x80000xxx           # 使用地址设置断点
```

### 2. 调试技巧

```bash
# 保存和恢复调试状态
(gdb) save breakpoints bp.txt
(gdb) source bp.txt

# 使用条件表达式
(gdb) print $pc > 0x80000000 && $pc < 0x80100000

# 查找特定内容
(gdb) find 0x80000000, 0x80100000, "hello"
```

## 总结

GDB 是调试 XV6 的强大工具，掌握其使用方法对于理解操作系统内部工作原理至关重要。本教程涵盖了：

1. **环境设置**: 工具安装和配置
2. **基础命令**: 断点、单步、查看等基本操作
3. **高级技巧**: 条件断点、自定义函数、脚本化调试
4. **实际应用**: 内核启动、系统调用、进程管理等场景
5. **故障排除**: 常见问题的解决方案

通过实践这些技巧，你将能够：
- 有效地调试 XV6 内核代码
- 理解操作系统的运行机制
- 快速定位和解决问题
- 深入学习系统编程

## 参考资料

- [GDB 官方文档](https://www.gnu.org/software/gdb/documentation/)
- [RISC-V GDB 使用指南](https://github.com/riscv/riscv-gnu-toolchain)
- [XV6 Book](https://pdos.csail.mit.edu/6.828/2021/xv6/book-riscv-rev2.pdf)
- [QEMU 调试文档](https://qemu.readthedocs.io/en/latest/system/gdb.html)