#include "types.h"     // 基本数据类型定义
#include "param.h"     // 系统参数定义（如NCPU等）
#include "memlayout.h" // 内存布局定义
#include "riscv.h"     // RISC-V架构相关定义和CSR操作函数
#include "defs.h"      // 函数声明

void main();      // 内核主函数声明
void timerinit(); // 定时器初始化函数声明

// entry.S needs one stack per CPU.
// entry.S 需要为每个CPU提供独立的栈空间
// 16字节对齐，为8个CPU各分配4KB栈空间，总共32KB
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// entry.S jumps here in machine mode on stack0.
// entry.S 在机器模式下跳转到这里，使用stack0作为栈
// 【函数功能】：内核启动函数，负责从Machine Mode切换到Supervisor Mode
// 主要任务：配置特权级、委托中断异常、设置内存保护、初始化定时器
void
start()
{
  // set M Previous Privilege mode to Supervisor, for mret.
  // 设置机器模式的前一特权级为监管者模式，为mret指令做准备
  // 当执行mret时，CPU将切换到MPP字段指定的特权级
  unsigned long x = r_mstatus();    // 读取机器状态寄存器
  x &= ~MSTATUS_MPP_MASK;           // 清除MPP字段（位[12:11]）
  x |= MSTATUS_MPP_S;               // 设置MPP为Supervisor模式（01）
  w_mstatus(x);                     // 写回mstatus寄存器

  // set M Exception Program Counter to main, for mret.
  // 设置机器异常程序计数器指向main函数，为mret指令做准备
  // requires gcc -mcmodel=medany
  // 需要gcc编译选项 -mcmodel=medany 以支持任意地址模型
  w_mepc((uint64)main);             // mret执行后将跳转到main函数

  // disable paging for now.
  // 暂时禁用分页机制
  // 将satp寄存器清零，禁用虚拟内存，此时使用物理地址直接访问
  w_satp(0);

  // delegate all interrupts and exceptions to supervisor mode.
  // 将所有中断和异常委托给监管者模式处理
  // 这样避免在机器模式下处理常规的操作系统事件，提高效率
  w_medeleg(0xffff);                     // 委托所有异常给Supervisor模式
  w_mideleg(0xffff);                     // 委托所有中断给Supervisor模式
  w_sie(r_sie() | SIE_SEIE | SIE_STIE);  // 启用Supervisor模式的外部中断和定时器中断

  // configure Physical Memory Protection to give supervisor mode
  // access to all of physical memory.
  // 配置物理内存保护，允许监管者模式访问所有物理内存
  // PMP用于限制低特权级的内存访问，这里配置为允许全部访问
  w_pmpaddr0(0x3fffffffffffffull);     // 设置PMP地址范围为整个物理地址空间
  w_pmpcfg0(0xf);                      // 设置PMP配置：读写执行权限，NAPOT模式

  // ask for clock interrupts.
  // 请求时钟中断
  // 初始化定时器系统，为操作系统调度和时间管理提供支持
  timerinit();

  // keep each CPU's hartid in its tp register, for cpuid().
  // 将每个CPU的hart ID保存在tp寄存器中，供cpuid()函数使用
  // Hart ID是RISC-V中CPU核心的唯一标识符
  int id = r_mhartid();                 // 读取当前CPU的Hart ID
  w_tp(id);                          // 将Hart ID写入线程指针寄存器

  // switch to supervisor mode and jump to main().
  // 切换到监管者模式并跳转到main()函数
  // mret指令将：1) 切换到MPP指定的特权级 2) 跳转到mepc指定的地址
  // 机器模式返回指令，完成特权级切换
  asm volatile("mret");             
}

// ask each hart to generate timer interrupts.
// 要求每个hart（CPU核心）生成定时器中断
//【函数功能】：初始化定时器中断系统
// 主要任务：启用定时器中断、配置SSTC扩展、设置访问权限、触发首次中断
void
timerinit()
{
  // enable supervisor-mode timer interrupts.
  // 启用监管者模式定时器中断
  // 在机器中断使能寄存器中启用Supervisor定时器中断位
  w_mie(r_mie() | MIE_STIE);        // MIE_STIE = (1L << 5)
  
  // enable the sstc extension (i.e. stimecmp).
  // 启用SSTC扩展（即stimecmp寄存器）
  // SSTC允许Supervisor模式直接使用stimecmp寄存器进行定时器比较
  w_menvcfg(r_menvcfg() | (1L << 63)); // 位63是STCE（Supervisor Timer Compare Enable） 
  
  // allow supervisor to use stimecmp and time.
  // 允许监管者模式使用stimecmp和time寄存器
  // mcounteren控制低特权级对计数器的访问权限
  w_mcounteren(r_mcounteren() | 2); // 位1对应time CSR的访问权限
  
  // ask for the very first timer interrupt.
  // 请求第一个定时器中断
  // 设置定时器比较值，当time >= stimecmp时触发中断
  w_stimecmp(r_time() + 1000000);   // 当前时间 + 1000000个时钟周期后中断
}
