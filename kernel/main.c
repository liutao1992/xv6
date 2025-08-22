#include "types.h"     // 基本数据类型定义
#include "param.h"     // 系统参数定义（如NCPU等）
#include "memlayout.h" // 内存布局定义
#include "riscv.h"     // RISC-V架构相关定义和CSR操作函数
#include "defs.h"      // 函数声明

// 多核同步标志：标记主CPU是否完成了系统初始化
// volatile确保编译器不会优化掉对该变量的访问
volatile static int started = 0;

// start() jumps here in supervisor mode on all CPUs.
// start() 在监管者模式下跳转到这里，所有CPU都会执行
// 【函数功能】：内核主初始化函数，负责系统各子系统的初始化
// 主要任务：多核启动协调、各子系统初始化、启动调度器
void
main()
{
  if(cpuid() == 0){  // 主CPU（Hart 0）负责系统初始化
    consoleinit();   // 初始化控制台输出系统，配置UART串口通信
                     // 使printf等输出函数能够正常工作
    
    printfinit();    // 初始化内核打印系统，设置printf的锁机制
                     // 确保多核环境下打印输出的线程安全
    
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    
    kinit();         // physical page allocator
                     // 初始化物理内存分配器，管理可用的物理页面
                     // 建立空闲页面链表，为后续内存分配提供基础
    
    kvminit();       // create kernel page table
                     // 创建内核页表，建立虚拟地址到物理地址的映射
                     // 为内核代码、数据、设备等建立地址空间
    
    kvminithart();   // turn on paging
                     // 启用分页机制，激活虚拟内存管理
                     // 设置satp寄存器，开始使用虚拟地址
    
    procinit();      // process table
                     // 初始化进程表，设置进程管理数据结构
                     // 为进程调度和管理做准备
    
    trapinit();      // trap vectors
                     // 初始化陷阱向量表，设置异常和中断处理入口
                     // 配置系统调用、页错误等的处理机制
    
    trapinithart();  // install kernel trap vector
                     // 为当前CPU安装内核陷阱向量
                     // 设置stvec寄存器指向内核陷阱处理程序
    
    plicinit();      // set up interrupt controller
                     // 初始化PLIC（Platform-Level Interrupt Controller）
                     // 配置外部设备中断的路由和优先级
    
    plicinithart();  // ask PLIC for device interrupts
                     // 为当前CPU配置PLIC，请求设备中断
                     // 启用外部中断的接收和处理
    
    binit();         // buffer cache
                     // 初始化缓冲区缓存系统，管理磁盘数据缓存
                     // 提供文件系统和磁盘之间的缓存层
    
    iinit();         // inode table
                     // 初始化inode表，管理文件系统的索引节点
                     // 为文件和目录的元数据管理做准备
    
    fileinit();      // file table
                     // 初始化文件表，管理打开的文件描述符
                     // 为文件操作系统调用提供支持
    
    virtio_disk_init(); // emulated hard disk
                        // 初始化virtio磁盘驱动，配置模拟硬盘设备
                        // 为文件系统提供底层存储支持
    
    userinit();      // first user process
                     // 创建第一个用户进程（init进程）
                     // 启动用户空间，开始用户程序的执行
    
    __sync_synchronize();  // 内存屏障，确保所有初始化操作完成
                           // 保证其他CPU能看到started变量的更新
    started = 1;           // 通知其他CPU主初始化已完成
  } else {  // 其他CPU（Hart 1-7）等待主CPU完成初始化
    while(started == 0)  // 忙等待，直到主CPU完成系统初始化
      ;                  // 确保系统基础设施已经建立
    
    __sync_synchronize();  // 内存屏障，确保能看到主CPU的所有初始化结果
                           // 保证内存操作的可见性和顺序性
    
    printf("hart %d starting\n", cpuid());  // 输出当前CPU的启动信息
    
    kvminithart();    // turn on paging
                      // 为当前CPU启用分页机制
                      // 设置satp寄存器，使用主CPU创建的页表
    
    trapinithart();   // install kernel trap vector
                      // 为当前CPU安装内核陷阱向量
                      // 设置stvec寄存器，配置异常处理
    
    plicinithart();   // ask PLIC for device interrupts
                      // 为当前CPU配置PLIC中断接收
                      // 启用外部设备中断的处理
  }

  scheduler();        // 启动进程调度器，开始调度用户进程
                      // 进入无限循环，寻找可运行的进程并执行
                      // 这是内核的最后一步，之后系统开始正常运行
}
