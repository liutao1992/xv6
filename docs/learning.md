# xv6-riscv 学习路线与练习指南

本指南面向在本仓库学习 xv6-riscv 的同学，提供系统化的学习路径、代码阅读顺序、动手练习、调试技巧与实验建议，帮助你从“能跑起来”到“深入理解内核关键路径”。

---

## 1. 目标与前提

- 目标：
  - 熟悉构建与运行流程，能稳定进行“修改-编译-运行-调试”的闭环
  - 理解启动主线、trap/系统调用路径、内存管理、调度、文件系统等模块
  - 通过小实验与练习获得对关键机制的“可观测性”和直觉

- 前提：
  - C 语言基础、基本的汇编概念（RISC-V 更佳）
  - 已能使用 make、在 QEMU 中运行 xv6（参考 docs/setup.md）

---

## 2. 学习路径（由浅入深）

- 阶段 1：构建与运行
  - 目标：能编译、运行、退出、调试
  - 行动：在项目根目录执行
    ```bash
    make clean
    make
    make qemu
    ```
    退出 QEMU：Ctrl + A 然后按 X

- 阶段 2：启动主线与系统调用路径
  - 目标：理解从开机到第一个用户进程，以及从用户态到内核态的系统调用处理链
  - 阅读建议：
    - kernel/main.c、kernel/trap.c、kernel/syscall.c
    - kernel/trampoline.S、kernel/kernelvec.S

- 阶段 3：内存与调度
  - 目标：理解物理页分配、页表、上下文切换、调度器
  - 阅读建议：
    - kernel/kalloc.c、kernel/vm.c、kernel/proc.c、kernel/swtch.S

- 阶段 4：文件系统与设备
  - 目标：理解缓冲、日志、inode/文件描述符、磁盘驱动
  - 阅读建议：
    - kernel/bio.c、kernel/log.c、kernel/fs.c、kernel/file.c、kernel/virtio_disk.c

---

## 3. 代码阅读顺序与要点

- 全局骨架
  - kernel/types.h、kernel/param.h：全局类型与常量（如 NPROC、NBUF、LOGSIZE）
  - kernel/defs.h：函数原型与模块间接口
  - Makefile：工具链前缀、QEMU 启动参数、UPROGS 用户程序列表

- 启动主线（从 main 开始）
  - 关注 init/kinit/kvminit/procinit/trapinit/plicinit 顺序
  - userinit 如何创建第一个用户进程

- 中断与系统调用
  - 用户态 ecall → trampoline 切到内核栈 → kernelvec 分发 → trap.c 判断来源
  - syscall.c 根据系统调用号派发 → 返回路径再经 trampoline 切回用户态

- 进程与调度
  - 进程结构体、上下文保存/恢复、调度循环、可抢占点

- 文件系统
  - bio 的缓冲策略、log 的写前日志、fs 的 inode 管理、file 的 fd 层封装
  - 与 virtio_disk 驱动的调用链

---

## 4. 动手练习（从易到难）

- 热身（1-2 小时）
  1) 扩展用户程序 ls（user/ls.c），打印更多信息（如 inode 或类型）
  2) 新增一个用户程序 hello（user/hello.c），打印 argv 并退出
     - 提示：需要在 Makefile 的 UPROGS 中加入 _hello，类似：
       ```
       UPROGS= ... _hello ...
       ```

- 自定义系统调用（半天）
  - 以“返回当前 CPU id”的 cpuid 系统调用为例：
    1) 在 kernel/syscall.h 增加 SYS_cpuid 的编号
    2) 在 kernel/syscall.c 注册并派发 cpuid
    3) 在 kernel/sysproc.c 实现 sys_cpuid（内部可调用内核的 cpuid()）
    4) 在 user/user.h 声明用户侧原型 int cpuid(void);
    5) 在 user/usys.pl 增加 entry("cpuid");
    6) 编写 user/cpuid.c 调用并打印
  - 通过该练习串起“号 → 派发 → 实现 → 用户封装 → 用户程序”的完整链路

- 进阶（1-2 天）
  1) ps：遍历进程表打印 pid、状态、内存大小等
  2) sleep 增强：基于 ticks 提供毫秒级 sleep，验证调度不会忙等
  3) 观察上下文切换：在关键点打印或统计，形成对调度行为的直觉

- 文件系统方向（1-2 天）
  1) 在 kernel/bio.c 统计缓存命中率（命中/不命中计数）
  2) 调整 NBUF、LOGSIZE，跑 user/stressfs.c，观察吞吐与命中率变化
  3) 阅读 kernel/fs.c 的日志提交点，理解崩溃一致性

---

## 5. 调试技巧

- 启动调试
  ```bash
  make qemu-gdb
  ```
  另开一个终端运行 gdb，常用命令：break、bt、si/ni、x/格式、watch

- 定点打印与断言
  - 对关键路径使用条件打印（如只在特定 pid 或第 N 次事件打印）
  - panic 只在确定严重错误时使用

- 关注点
  - trap.c：时钟中断 devintr/timer 路径
  - proc.c/swtch.S：sched/上下文切换
  - vm.c：walk/mappages，页表映射与页故障时的 scause/stval

- 回归与自测
  - 修改后运行 user/usertests.c 做基础回归，避免无意回退

---

## 6. 实验与测量建议

- 上下文切换成本
  - 在切换前后记录 ticks 或时间戳，估算开销

- 自旋锁竞争
  - 在 kernel/spinlock.c 中为 acquire/release 增加争用统计，构造压力测试

- I/O 缓存效果
  - 调整 NBUF/LOGSIZE；对比顺序/随机读写的命中率与吞吐

- 可观测性提升
  - 为关键模块增加可开关的统计与调试接口（避免常驻影响时序）

---

## 7. 两周学习计划样例

- 第 1-2 天：环境搭建、阅读骨架与主线；完成 hello、扩展 ls
- 第 3-5 天：深入 trap/syscall 路径；新增 1-2 个自定义系统调用；学会 gdb 单步/回溯
- 第 6-8 天：理解 vm/proc；实现 ps 或 sleep 增强；观察上下文切换
- 第 9-12 天：理解 fs/bio/log；做一次缓存命中率或日志提交点实验；记实验笔记
- 第 13-14 天：整理报告与调试套路清单；思考后续目标（如 COW、调度策略等）

---

## 8. 常见坑与避坑

- 工具链前缀/版本不匹配
  - 通过 make TOOLPREFIX=riscv64-elf- 或 riscv64-unknown-elf- 指定
  - 确保 QEMU 版本足够新

- 死锁/关中断忘开
  - 打印锁名/持有者 pid；必要时用 gdb 观察队列与状态

- 打印过多扰动时序
  - 关键路径采用条件打印；先用 gdb 观察，再补打印确认

- 用户/内核态边界不清晰
  - trap.c 中明确 scause、SSTATUS 标志位与转换流程

---

## 9. 命令速查

```bash
make clean
```
```bash
make
```
```bash
make qemu
```
```bash
make qemu-gdb
```

退出 QEMU：Ctrl + A，然后 X

---

## 10. 建议的工程习惯

- 小步提交：一次只改一个点，写清“动机-修改-影响”
- 画图与流程：把“启动主线”“trap/syscall”“页表层次”“文件读写路径”画成笔记
- 调试套路清单：不同类型问题（死锁/页故障/panic）建立定位 checklist
- 保持回归：每次功能改动都跑 usertests 与关键用例