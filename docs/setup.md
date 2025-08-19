# xv6-riscv 环境搭建与运行指南

本文档面向在 macOS M4（Apple Silicon）平台进行 xv6-riscv 学习与开发的同学，提供三种推荐环境：
- 直接在 macOS 上安装 RISC-V 工具链与 QEMU
- 在 Ubuntu 虚拟机中搭建（如 UTM / Parallels / VMware）
- 使用 Multipass 快速创建轻量级 Ubuntu 虚拟机

并提供构建运行、调试与常见问题排查。

---

## 目录
- 项目简介
- 代码目录结构概览
- 构建与运行
- macOS M4 原生环境搭建
- 在 Ubuntu 虚拟机中搭建
- 使用 Multipass 搭建 Ubuntu 环境
- 调试与常见问题
- 附录：常用命令速查

---

## 项目简介
- 项目：xv6-riscv
- 定位：MIT 6.1810 操作系统课程教学用内核，参考 Unix v6，RISC-V 64 平台
- 运行：通过 QEMU 启动模拟器运行
- 学习：内核启动、调度、内存、文件系统、系统调用、中断、驱动等

---

## 代码目录结构概览
- 根目录
  - Makefile：构建流程、工具链前缀探测、QEMU 启动参数
  - README：项目说明
- kernel/
  - 启动与陷阱：entry.S、start.c、trampoline.S、kernelvec.S、trap.c
  - 初始化与调度：main.c、proc.c、swtch.S
  - 内存管理：vm.c、kalloc.c、memlayout.h
  - 文件与设备：fs.c、file.c、bio.c、log.c、virtio_disk.c、uart.c
  - 同步：spinlock.c、sleeplock.c
  - 系统调用：syscall.c、sysproc.c、sysfile.c
  - 公共：types.h、param.h、defs.h、printf.c、string.c
- user/
  - 程序：sh.c、ls.c、cat.c、echo.c、grep.c、mkdir.c、rm.c、wc.c、kill.c、ln.c、init.c、zombie.c 等
  - 支持库：ulib.c、umalloc.c、printf.c、user.h、user.ld
  - 系统调用生成：usys.pl

---

## 构建与运行（仓库根目录）
工具链与 QEMU 要求（Makefile 会自动探测以下前缀之一）：
- 工具链前缀：riscv64-unknown-elf- / riscv64-elf- / riscv64-linux-gnu- / riscv64-unknown-linux-gnu-
- 建议 QEMU 版本 ≥ 7.2

常用命令：
```bash
# 清理
make clean
```
```bash
# 编译
make
```
```bash
# 运行
make qemu
```
```bash
# 调试：一个终端
make qemu-gdb
```
```bash
# 调试：另一个终端
gdb
```
退出 QEMU：在 QEMU 控制台按 Ctrl+A，然后按 X。

---

## macOS M4 原生环境搭建

1) 安装 Homebrew（若已安装可跳过）
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

2) 安装 RISC-V 工具链（推荐）
```bash
brew install riscv64-elf-gcc
```

3) 安装 QEMU（建议 ≥ 7.2）
```bash
brew install qemu
```

4) 验证
```bash
riscv64-elf-gcc --version
```
```bash
riscv64-unknown-elf-gcc --version
```
```bash
qemu-system-riscv64 --version
```

5) 构建与运行（仓库根目录）
```bash
make clean
```
```bash
make
```
```bash
make qemu
```

常见问题：
- 找不到工具链前缀（TOOLPREFIX）：
```bash
make TOOLPREFIX=riscv64-elf-
```
或
```bash
make TOOLPREFIX=riscv64-unknown-elf-
```
- QEMU 版本过低：
```bash
brew upgrade qemu
```

---

## 在 Ubuntu 虚拟机中搭建（UTM / Parallels / VMware）

在虚拟机中：
```bash
sudo apt update
sudo apt upgrade -y
sudo apt install -y build-essential git vim curl wget
sudo apt install -y autoconf automake autotools-dev python3 python3-pip
sudo apt install -y libmpc-dev libmpfr-dev libgmp-dev gawk bison flex texinfo
sudo apt install -y gperf libtool patchutils bc zlib1g-dev libexpat-dev
```

安装 RISC-V 工具链（推荐包管理器）：
```bash
sudo apt install -y gcc-riscv64-unknown-elf
riscv64-unknown-elf-gcc --version
```

安装 QEMU：
```bash
sudo apt install -y qemu-system-misc
qemu-system-riscv64 --version
```

获取并运行：
```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make
make qemu
```

性能建议：分配 4+ CPU / 4G+ 内存、使用 SSD、关闭不必要服务（snapd/bluetooth 等）。

---

## 使用 Multipass 搭建 Ubuntu 环境（macOS 上）

在 macOS 主机：
```bash
brew install --cask multipass
```

创建并进入虚拟机：
```bash
multipass launch --name xv6-dev --memory 4G --cpus 2 --disk 20G
multipass list
multipass shell xv6-dev
```

在虚拟机内安装依赖与工具链：
```bash
sudo apt update && sudo apt upgrade -y
sudo apt install -y build-essential git vim curl wget
sudo apt install -y autoconf automake autotools-dev python3 python3-pip
sudo apt install -y libmpc-dev libmpfr-dev libgmp-dev gawk bison flex texinfo
sudo apt install -y gperf libtool patchutils bc zlib1g-dev libexpat-dev
sudo apt install -y gcc-riscv64-unknown-elf
sudo apt install -y qemu-system-misc
riscv64-unknown-elf-gcc --version
qemu-system-riscv64 --version
```

获取并运行：
```bash
git clone https://github.com/mit-pdos/xv6-riscv.git
cd xv6-riscv
make
make qemu
```

共享目录（推荐工作流，在 macOS 主机执行）：
```bash
multipass mount /Users/liutao/Github/xv6-riscv xv6-dev:/home/ubuntu/xv6-riscv
```
在虚拟机内：
```bash
cd /home/ubuntu/xv6-riscv
make
make qemu
```

Multipass 常用命令（在 macOS 主机）：
```bash
multipass info xv6-dev
```
```bash
multipass start xv6-dev
```
```bash
multipass stop xv6-dev
```
```bash
multipass restart xv6-dev
```
```bash
multipass delete xv6-dev
multipass purge
```

---

## 调试与常见问题

- GDB 调试
  - 终端 1：
    ```bash
    make qemu-gdb
    ```
  - 终端 2：
    ```bash
    gdb
    ```

- 多核数目（默认 3 个 CPU，可按需调整）：
  ```bash
  CPUS=2 make qemu
  ```

- 工具链前缀不匹配：
  ```bash
  make TOOLPREFIX=riscv64-elf-
  ```
  或
  ```bash
  make TOOLPREFIX=riscv64-unknown-elf-
  ```

- QEMU 版本检查/升级（Ubuntu）：
  ```bash
  qemu-system-riscv64 --version
  sudo apt install -y qemu-system-misc
  ```

- 共享目录权限（在 Linux 虚拟机）：
  ```bash
  sudo chown -R ubuntu:ubuntu /home/ubuntu/xv6-riscv
  ```

---

## 附录：常用命令速查
```bash
make clean
make
make qemu
make qemu-gdb
```
退出 QEMU：Ctrl+A，然后 X。
