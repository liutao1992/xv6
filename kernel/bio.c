// Buffer cache.
// 缓冲区缓存。
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
// 缓冲区缓存是一个由buf结构组成的链表，保存磁盘块内容的缓存副本。
// 在内存中缓存磁盘块可以减少磁盘读取次数，同时为多个进程使用的磁盘块
// 提供同步点。
//
// Interface:
// 接口：
// * To get a buffer for a particular disk block, call bread.
// * 要获取特定磁盘块的缓冲区，调用bread。
// * After changing buffer data, call bwrite to write it to disk.
// * 修改缓冲区数据后，调用bwrite将其写入磁盘。
// * When done with the buffer, call brelse.
// * 使用完缓冲区后，调用brelse。
// * Do not use the buffer after calling brelse.
// * 调用brelse后不要再使用该缓冲区。
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
// * 同一时间只能有一个进程使用一个缓冲区，
//   所以不要保持它们超过必要的时间。


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // 通过prev/next指针连接的所有缓冲区的链表。
  // 按照缓冲区最近使用时间排序。
  // head.next是最近使用的，head.prev是最久未使用的。
  struct buf head;
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  // 创建缓冲区链表
  // 
  // 初始状态: head <-> head (空链表) head节点指向自己形成空的循环链表
  // 插入缓冲区后的结构:
  //   head <-> buf[NBUF-1] <-> buf[NBUF-2] <-> ... <-> buf[1] <-> buf[0] <-> head
  //   ↑                                                                      ↑
  //   └──────────────────────────────────────────────────────────────────────┘
  //   (最新使用的)                                              (最久未使用的)
  //
  bcache.head.prev = &bcache.head;
  bcache.head.next = &bcache.head;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
// 在缓冲区缓存中查找设备dev上的块。
// 如果找不到，分配一个缓冲区。
// 无论哪种情况，都返回已锁定的缓冲区。
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  acquire(&bcache.lock);

  // Is the block already cached?
  // 该块是否已经被缓存？
  for(b = bcache.head.next; b != &bcache.head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // 未缓存。
  // 回收最近最少使用(LRU)的未使用缓冲区。
  for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
// 返回包含指定块内容的已锁定buf。
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
// 将b的内容写入磁盘。必须已锁定。
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
// 释放已锁定的缓冲区。
// 移动到最近使用列表的头部。
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  acquire(&bcache.lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    // 没有人在等待它。
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head.next;
    b->prev = &bcache.head;
    bcache.head.next->prev = b;
    bcache.head.next = b;
  }
  
  release(&bcache.lock);
}

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


