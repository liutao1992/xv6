好嘞 👍 我来给你整理一个 **《xv6 学习速查表》**，把 **书本章节 ↔ 核心源码文件 ↔ 对应 MIT Lab** 对应起来，你就可以顺着走：

---

# 📖 xv6 学习速查表

| 主题         | 书本章节                             | 核心源码文件                                | 实验（Lab）                                  |
| ---------- | -------------------------------- | ------------------------------------- | ---------------------------------------- |
| **操作系统接口** | Ch.1 Operating system interfaces | `user/ls.c`, `user/sh.c`              | ——（跑一下命令体验）                              |
| **进程与调度**  | Ch.2, Ch.3 Processes             | `proc.c`, `proc.h`, `sysproc.c`       | Lab1: `syscall` / `traps`（实现 trace 系统调用） |
| **内存管理**   | Ch.4 Page tables                 | `vm.c`, `kalloc.c`, `exec.c`          | Lab2: `pgtbl`（虚拟内存实验）                    |
| **系统调用机制** | Ch.2 + 各章节补充                     | `syscall.c`, `sysfile.c`, `usys.S`    | Lab1: 添加系统调用（`getpid()`）                 |
| **并发 & 锁** | Ch.5, Ch.8 Locking               | `spinlock.c`, `sleeplock.c`, `proc.c` | Lab3: `cow`（Copy-on-Write Fork）          |
| **文件系统**   | Ch.6, Ch.7 File system           | `fs.c`, `file.c`, `log.c`, `bio.c`    | Lab4: `fs`（大文件 / 软链接）                    |
| **进程通信**   | Ch.5（pipe 部分）                    | `pipe.c`, `sysfile.c`                 | Lab5: `xargs`, `find`（用户程序练习）            |
| **设备驱动**   | Ch.7, Ch.9 I/O                   | `uart.c`, `console.c`                 | Lab6: `net`（选做，网络协议）                     |
| **高级主题**   | Ch.10                            | （综合）                                  | Lab7: `threads`（用户线程库）                   |

---

## 🔑 使用方法

1. **读书 → 找源码 → 做实验**
   比如学到 **进程调度**：

   * 看书 Ch.3 →
   * 对照 `proc.c` →
   * 做 `trace` / `syscall` 实验。

2. **做笔记时写下“章节 ↔ 文件”映射**
   以后想复习某个功能，比如 “文件系统”，直接看 `fs.c`，不用翻来翻去。

3. **实验优先顺序**（MIT labs）：

   * Lab1 syscall/traps
   * Lab2 pgtbl (内存)
   * Lab3 cow (写时复制)
   * Lab4 fs (文件系统)
   * Lab5 user programs (find/xargs)
   * Lab6+7 并发 & 网络

---


