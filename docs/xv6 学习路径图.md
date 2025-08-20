太好了 👍 我给你整理一个 **xv6 学习路径图**，按照 **“书本 → 源码 → 实验”** 的顺序来走，比较高效：

---

# 🛠 xv6 学习路径图

## 1️⃣ 基础准备

* **目标**：能跑起来 xv6，知道它是个“简化版 Unix”。
* **步骤**：

  1. 下载 xv6-riscv 源码 → [GitHub](https://github.com/mit-pdos/xv6-riscv)
  2. 安装 QEMU，能 `make qemu` 启动。
  3. 读书 **Chapter 1: Operating system interfaces**，跑书里给的命令体验下。

---

## 2️⃣ 进程管理（Process）

* **书本**：Chapter 2 & 3
* **源码**：`proc.c`、`proc.h`、`sysproc.c`
* **关键点**：进程创建 (`fork`)、调度 (`scheduler`)、退出 (`exit`)。
* **实验（lab1）**：实现一个新系统调用，比如 `trace`，在进程执行时打印调用的系统调用号。

---

## 3️⃣ 内存管理（Memory Management）

* **书本**：Chapter 4
* **源码**：`vm.c`、`kalloc.c`、`exec.c`
* **关键点**：页表、用户/内核地址空间、内存分配。
* **实验（lab2）**：实现用户内存分配限制，比如 `sbrk()` 的限制。

---

## 4️⃣ 系统调用（System Calls）

* **书本**：Chapter 2（接口）+ 各章节案例
* **源码**：`syscall.c`、`sysfile.c`、`usys.S`
* **关键点**：用户态 → 内核态的切换过程。
* **实验（lab3）**：增加一个 `getpid()` 或 `uptime()` 系统调用。

---

## 5️⃣ 文件系统（File System）

* **书本**：Chapter 6 & 7
* **源码**：`fs.c`、`file.c`、`log.c`
* **关键点**：inode、目录、磁盘日志。
* **实验（lab4）**：实现大文件支持（超过 12 个 direct block）。

---

## 6️⃣ 并发与锁（Concurrency & Synchronization）

* **书本**：Chapter 5 & 8
* **源码**：`spinlock.c`、`sleeplock.c`、`proc.c`（调度器部分）
* **关键点**：自旋锁、睡眠锁、死锁避免。
* **实验（lab5）**：实现一个内核线程库，体验多核并发。

---

## 7️⃣ 高级主题

* **书本**：Chapter 9–10（网络、设备）
* **源码**：`pipe.c`、`sysfile.c`、`uart.c`
* **实验（lab6）**：实现一个简单的网络协议（MIT labs 有选做）。

---

# 🔑 学习技巧

* **边学边改代码**：比如加个系统调用 `hello()`，打印字符串。
* **加调试输出**：用 `cprintf()` 观察内核流程。
* **做实验**：MIT labs 设计得非常好，每个实验都直击重点。
* **形成“文件-章节”映射表**：以后查源码就不会迷路。

---


