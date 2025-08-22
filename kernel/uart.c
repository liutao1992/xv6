//
// low-level driver routines for 16550a UART.
// 16550a UART 的低级驱动程序例程。
//

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// the UART control registers are memory-mapped
// UART 控制寄存器被内存映射
// at address UART0. this macro returns the
// 到地址 UART0。这个宏返回
// address of one of the registers.
// 其中一个寄存器的地址。
#define Reg(reg) ((volatile unsigned char *)(UART0 + (reg)))

// the UART control registers.
// UART 控制寄存器。
// some have different meanings for
// 有些在读取和写入时有不同的含义。
// read vs write.
// see http://byterunner.com/16550.html
// 参见 http://byterunner.com/16550.html
#define RHR 0                 // receive holding register (for input bytes)
                              // 接收保持寄存器（用于输入字节）
#define THR 0                 // transmit holding register (for output bytes)
                              // 发送保持寄存器（用于输出字节）
#define IER 1                 // interrupt enable register
                              // 中断使能寄存器
#define IER_RX_ENABLE (1<<0)
#define IER_TX_ENABLE (1<<1)
#define FCR 2                 // FIFO control register
                              // FIFO 控制寄存器
#define FCR_FIFO_ENABLE (1<<0)
#define FCR_FIFO_CLEAR (3<<1) // clear the content of the two FIFOs
                              // 清除两个 FIFO 的内容
#define ISR 2                 // interrupt status register
                              // 中断状态寄存器
#define LCR 3                 // line control register
                              // 线路控制寄存器
#define LCR_EIGHT_BITS (3<<0)
#define LCR_BAUD_LATCH (1<<7) // special mode to set baud rate
                              // 设置波特率的特殊模式
#define LSR 5                 // line status register
                              // 线路状态寄存器
#define LSR_RX_READY (1<<0)   // input is waiting to be read from RHR
                              // 输入正在等待从 RHR 读取
#define LSR_TX_IDLE (1<<5)    // THR can accept another character to send
                              // THR 可以接受另一个要发送的字符

#define ReadReg(reg) (*(Reg(reg)))
#define WriteReg(reg, v) (*(Reg(reg)) = (v))

// the transmit output buffer.
// 发送输出缓冲区。
struct spinlock uart_tx_lock;
#define UART_TX_BUF_SIZE 32
char uart_tx_buf[UART_TX_BUF_SIZE];
uint64 uart_tx_w; // write next to uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
                  // 写入下一个到 uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE]
uint64 uart_tx_r; // read next from uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE]
                  // 从 uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE] 读取下一个

extern volatile int panicking; // from printf.c
                               // 来自 printf.c
extern volatile int panicked; // from printf.c
                              // 来自 printf.c

void uartstart();

void
uartinit(void)
{
  // disable interrupts.
  // 禁用中断。
  WriteReg(IER, 0x00);

  // special mode to set baud rate.
  // 设置波特率的特殊模式。
  WriteReg(LCR, LCR_BAUD_LATCH);

  // LSB for baud rate of 38.4K.
  // 38.4K 波特率的低字节。
  WriteReg(0, 0x03);

  // MSB for baud rate of 38.4K.
  // 38.4K 波特率的高字节。
  WriteReg(1, 0x00);

  // leave set-baud mode,
  // 退出设置波特率模式，
  // and set word length to 8 bits, no parity.
  // 并设置字长为 8 位，无奇偶校验。
  WriteReg(LCR, LCR_EIGHT_BITS);

  // reset and enable FIFOs.
  // 重置并启用 FIFO。
  WriteReg(FCR, FCR_FIFO_ENABLE | FCR_FIFO_CLEAR);

  // enable transmit and receive interrupts.
  // 启用发送和接收中断。
  WriteReg(IER, IER_TX_ENABLE | IER_RX_ENABLE);

  initlock(&uart_tx_lock, "uart");
}

// add a character to the output buffer and tell the
// 将字符添加到输出缓冲区并告诉
// UART to start sending if it isn't already.
// UART 开始发送（如果尚未开始）。
// blocks if the output buffer is full.
// 如果输出缓冲区已满则阻塞。
// because it may block, it can't be called
// 因为它可能阻塞，所以不能从
// from interrupts; it's only suitable for use
// 中断中调用；它只适合
// by write().
// write() 使用。
void
uartputc(int c)
{
  if(panicking == 0)
    acquire(&uart_tx_lock);

  if(panicked){
    for(;;)
      ;
  }
  while(uart_tx_w == uart_tx_r + UART_TX_BUF_SIZE){
    // buffer is full.
    // 缓冲区已满。
    // wait for uartstart() to open up space in the buffer.
    // 等待 uartstart() 在缓冲区中腾出空间。
    sleep(&uart_tx_r, &uart_tx_lock);
  }
  uart_tx_buf[uart_tx_w % UART_TX_BUF_SIZE] = c;
  uart_tx_w += 1;
  uartstart();
  if(panicking == 0)
    release(&uart_tx_lock);
}


// alternate version of uartputc() that doesn't 
// uartputc() 的替代版本，不使用
// use interrupts, for use by kernel printf() and
// 中断，供内核 printf() 和
// to echo characters. it spins waiting for the uart's
// 回显字符使用。它自旋等待 uart 的
// output register to be empty.
// 输出寄存器为空。
void
uartputc_sync(int c)
{
  if(panicking == 0)
    push_off();

  if(panicked){
    for(;;)
      ;
  }

  // wait for Transmit Holding Empty to be set in LSR.
  // 等待 LSR 中的发送保持空标志被设置。
  while((ReadReg(LSR) & LSR_TX_IDLE) == 0)
    ;
  WriteReg(THR, c);

  if(panicking == 0)
    pop_off();
}

// if the UART is idle, and a character is waiting
// 如果 UART 空闲，并且有字符在
// in the transmit buffer, send it.
// 发送缓冲区中等待，则发送它。
// caller must hold uart_tx_lock.
// 调用者必须持有 uart_tx_lock。
// called from both the top- and bottom-half.
// 从上半部和下半部都会调用。
void
uartstart()
{
  while(1){
    if(uart_tx_w == uart_tx_r){
      // transmit buffer is empty.
      // 发送缓冲区为空。
      return;
    }
    
    if((ReadReg(LSR) & LSR_TX_IDLE) == 0){
      // the UART transmit holding register is full,
      // UART 发送保持寄存器已满，
      // so we cannot give it another byte.
      // 所以我们不能给它另一个字节。
      // it will interrupt when it's ready for a new byte.
      // 当它准备好接收新字节时会产生中断。
      return;
    }
    
    int c = uart_tx_buf[uart_tx_r % UART_TX_BUF_SIZE];
    uart_tx_r += 1;
    
    // maybe uartputc() is waiting for space in the buffer.
    // 也许 uartputc() 正在等待缓冲区中的空间。
    wakeup(&uart_tx_r);
    
    WriteReg(THR, c);
  }
}

// read one input character from the UART.
// 从 UART 读取一个输入字符。
// return -1 if none is waiting.
// 如果没有等待的字符则返回 -1。
int
uartgetc(void)
{
  if(ReadReg(LSR) & LSR_RX_READY){
    // input data is ready.
    // 输入数据已准备好。
    return ReadReg(RHR);
  } else {
    return -1;
  }
}

// handle a uart interrupt, raised because input has
// 处理 uart 中断，因为有输入
// arrived, or the uart is ready for more output, or
// 到达，或者 uart 准备好更多输出，或者
// both. called from devintr().
// 两者都有。从 devintr() 调用。
void
uartintr(void)
{
  ReadReg(ISR); // acknowledge the interrupt
                // 确认中断

  // read and process incoming characters.
  // 读取并处理传入的字符。
  while(1){
    int c = uartgetc();
    if(c == -1)
      break;
    consoleintr(c);
  }

  // send buffered characters.
  // 发送缓冲的字符。
  if(panicking == 0)
    acquire(&uart_tx_lock);
  uartstart();
  if(panicking == 0)
    release(&uart_tx_lock);
}
