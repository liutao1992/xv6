//
// Console input and output, to the uart.
// 控制台输入输出，连接到 UART。
// Reads are line at a time.
// 按行读取数据。
// Implements special input characters:
// 实现特殊输入字符：
//   newline -- end of line
//   换行符 -- 行结束
//   control-h -- backspace
//   control-h -- 退格
//   control-u -- kill line
//   control-u -- 删除行
//   control-d -- end of file
//   control-d -- 文件结束
//   control-p -- print process list
//   control-p -- 打印进程列表
//

#include <stdarg.h>

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

#define BACKSPACE 0x100
#define C(x)  ((x)-'@')  // Control-x
                         // 控制字符宏定义

//
// send one character to the uart.
// 向 UART 发送一个字符。
// called by printf(), and to echo input characters,
// 被 printf() 调用，用于回显输入字符，
// but not from write().
// 但不被 write() 调用。
//
// 函数设计：向 UART 发送单个字符的核心函数
// 实现细节：
// 1. 处理退格字符的特殊显示：发送退格-空格-退格序列来清除字符
// 2. 普通字符直接通过 uartputc_sync() 同步发送
// 3. 被 printf() 和字符回显调用，但不被 write() 系统调用直接使用
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    // 如果用户输入退格，用空格覆盖。
    // 退格处理：先退格，再打印空格覆盖，最后再退格到正确位置
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    // 普通字符直接发送到 UART
    uartputc_sync(c);
  }
}

struct {
  struct spinlock lock;
  
  // input
  // 输入相关
#define INPUT_BUF_SIZE 128
  char buf[INPUT_BUF_SIZE];
  uint r;  // Read index
           // 读索引
  uint w;  // Write index
           // 写索引
  uint e;  // Edit index
           // 编辑索引
} cons;

//
// user write()s to the console go here.
// 用户对控制台的 write() 调用在此处理。
//
// 函数设计：处理用户程序向控制台写入数据的系统调用接口
// 实现细节：
// 1. 逐字节从用户空间复制数据到内核空间
// 2. 使用 either_copyin() 安全地处理用户/内核地址空间转换
// 3. 通过 uartputc() 异步发送字符到 UART
// 4. 返回实际写入的字节数，出错时返回已写入的字节数
int
consolewrite(int user_src, uint64 src, int n)
{
  int i;

  // 逐字节处理写入请求
  for(i = 0; i < n; i++){
    char c;
    // 从用户空间安全复制一个字节到内核空间
    if(either_copyin(&c, user_src, src+i, 1) == -1)
      break;  // 复制失败则停止
    // 异步发送字符到 UART（与 consputc 不同，这里用异步版本）
    uartputc(c);
  }

  return i;  // 返回实际写入的字节数
}

//
// user read()s from the console go here.
// 用户从控制台的 read() 调用在此处理。
// copy (up to) a whole input line to dst.
// 复制（最多）一整行输入到目标地址。
// user_dist indicates whether dst is a user
// user_dst 指示目标地址是用户空间
// or kernel address.
// 还是内核地址。
//
// 函数设计：处理用户程序从控制台读取数据的系统调用接口
// 实现细节：
// 1. 行缓冲模式：等待完整行输入或遇到特殊字符才返回
// 2. 使用环形缓冲区 cons.buf 存储输入字符
// 3. 通过睡眠/唤醒机制等待输入数据
// 4. 处理 Ctrl+D (EOF) 特殊情况
// 5. 安全地将数据从内核空间复制到用户空间
int
consoleread(int user_dst, uint64 dst, int n)
{
  uint target;
  int c;
  char cbuf;

  target = n;  // 保存原始请求字节数
  acquire(&cons.lock);  // 获取控制台锁保护并发访问
  
  while(n > 0){
    // wait until interrupt handler has put some
    // 等待中断处理程序放入一些
    // input into cons.buffer.
    // 输入到控制台缓冲区。
    // 等待中断处理程序将输入放入缓冲区
    while(cons.r == cons.w){  // 缓冲区为空时等待
      if(killed(myproc())){   // 检查进程是否被杀死
        release(&cons.lock);
        return -1;
      }
      sleep(&cons.r, &cons.lock);  // 睡眠等待输入，释放锁
    }

    // 从环形缓冲区读取一个字符
    c = cons.buf[cons.r++ % INPUT_BUF_SIZE];

    if(c == C('D')){  // end-of-file (Ctrl+D)
                      // 文件结束符 (Ctrl+D)
      if(n < target){
        // Save ^D for next time, to make sure
        // 保存 ^D 供下次使用，确保
        // caller gets a 0-byte result.
        // 调用者得到 0 字节结果。
        // 如果已读取了部分数据，保存 Ctrl+D 供下次使用
        cons.r--;
      }
      break;  // EOF 时结束读取
    }

    // copy the input byte to the user-space buffer.
    // 复制输入字节到用户空间缓冲区。
    // 将字符安全复制到用户空间
    cbuf = c;
    if(either_copyout(user_dst, dst, &cbuf, 1) == -1)
      break;  // 复制失败则停止

    dst++;   // 移动目标指针
    --n;     // 减少剩余字节数

    if(c == '\n'){
      // a whole line has arrived, return to
      // 完整的一行已到达，返回到
      // the user-level read().
      // 用户级 read()。
      // 遇到换行符，完整行已到达，返回给用户程序
      break;
    }
  }
  release(&cons.lock);  // 释放控制台锁

  return target - n;  // 返回实际读取的字节数
}

//
// the console input interrupt handler.
// 控制台输入中断处理程序。
// uartintr() calls this for input character.
// uartintr() 为输入字符调用此函数。
// do erase/kill processing, append to cons.buf,
// 进行擦除/删除处理，追加到 cons.buf，
// wake up consoleread() if a whole line has arrived.
// 如果完整行到达则唤醒 consoleread()。
//
// 函数设计：控制台输入中断处理程序，处理来自 UART 的字符输入
// 实现细节：
// 1. 处理特殊控制字符（Ctrl+P, Ctrl+U, Ctrl+H, Delete）
// 2. 维护三个缓冲区索引：r(读)、w(写)、e(编辑)
// 3. 实现字符回显和行编辑功能
// 4. 在完整行到达时唤醒等待的读取进程
void
consoleintr(int c)
{
  acquire(&cons.lock);  // 获取控制台锁保护并发访问

  switch(c){
  case C('P'):  // Print process list. (Ctrl+P)
                // 打印进程列表。(Ctrl+P)
    // 打印进程列表，用于调试
    procdump();
    break;
  case C('U'):  // Kill line. (Ctrl+U)
                // 删除行。(Ctrl+U)
    // 删除当前行：从编辑位置向前删除到行首或写位置
    while(cons.e != cons.w &&
          cons.buf[(cons.e-1) % INPUT_BUF_SIZE] != '\n'){
      cons.e--;           // 回退编辑索引
      consputc(BACKSPACE); // 在屏幕上显示退格
    }
    break;
  case C('H'): // Backspace (Ctrl+H)
               // 退格 (Ctrl+H)
  case '\x7f': // Delete key
               // 删除键
    // 退格删除：删除一个字符
    if(cons.e != cons.w){  // 确保有字符可删除
      cons.e--;           // 回退编辑索引
      consputc(BACKSPACE); // 在屏幕上显示退格
    }
    break;
  default:
    // 处理普通字符输入
    if(c != 0 && cons.e-cons.r < INPUT_BUF_SIZE){  // 缓冲区未满
      // 将回车转换为换行符
      c = (c == '\r') ? '\n' : c;

      // echo back to the user.
      // 回显给用户。
      // 字符回显：在屏幕上显示输入的字符
      consputc(c);

      // store for consumption by consoleread().
      // 存储供 consoleread() 使用。
      // 将字符存储到环形缓冲区供 consoleread() 使用
      cons.buf[cons.e++ % INPUT_BUF_SIZE] = c;

      // 检查是否需要唤醒等待的读取进程
      if(c == '\n' || c == C('D') || cons.e-cons.r == INPUT_BUF_SIZE){
        // wake up consoleread() if a whole line (or end-of-file)
        // 如果完整行（或文件结束）
        // has arrived.
        // 已到达，则唤醒 consoleread()。
        // 完整行、EOF 或缓冲区满时，更新写索引并唤醒读取进程
        cons.w = cons.e;
        wakeup(&cons.r);
      }
    }
    break;
  }
  
  release(&cons.lock);  // 释放控制台锁
}

// 函数设计：控制台初始化函数，设置控制台子系统
// 实现细节：
// 1. 初始化控制台自旋锁保护并发访问
// 2. 初始化底层 UART 硬件
// 3. 在设备表中注册控制台的读写函数指针
// 4. 使控制台成为系统中 CONSOLE 设备的处理程序
void
consoleinit(void)
{
  // 初始化控制台锁，名称为 "cons"
  initlock(&cons.lock, "cons");

  // 初始化 UART 硬件
  uartinit();

  // connect read and write system calls
  // 连接读写系统调用
  // to consoleread and consolewrite.
  // 到 consoleread 和 consolewrite。
  // 在设备表中注册控制台的读写函数，使系统调用能够找到对应处理函数
  devsw[CONSOLE].read = consoleread;   // 注册读函数
  devsw[CONSOLE].write = consolewrite; // 注册写函数
}
