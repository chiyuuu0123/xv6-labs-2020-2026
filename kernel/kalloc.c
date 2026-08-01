// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem_cpu {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
// 把物理页放回当前 CPU 的本地空闲链表，减少不同 CPU 对同一把锁的竞争。
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // 关闭中断以保证取得 cpuid 后当前执行不会迁移到另一 CPU。
  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
// 优先从本地 freelist 分配；耗尽时再从其他 CPU 批量转移页面。
void *
kalloc(void)
{
  struct run *r = 0;

  push_off();
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  if(r == 0){
    // 本地链表耗尽时，从其他 CPU 批量偷取约一半，降低后续跨 CPU 竞争频率。
    for(int donor = 0; donor < NCPU && r == 0; donor++){
      if(donor == id)
        continue;
      acquire(&kmem[donor].lock);
      int count = 0;
      for(struct run *p = kmem[donor].freelist; p; p = p->next)
        count++;
      int take = count / 2;
      if(take == 0 && count > 0)
        take = 1;
      if(take > 0){
        struct run *first = kmem[donor].freelist;
        struct run *last = first;
        for(int i = 1; i < take; i++)
          last = last->next;
        kmem[donor].freelist = last->next;
        last->next = 0;
        release(&kmem[donor].lock);

        acquire(&kmem[id].lock);
        last->next = kmem[id].freelist;
        kmem[id].freelist = first->next;
        r = first;
        release(&kmem[id].lock);
      } else {
        release(&kmem[donor].lock);
      }
    }
  }
  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
