// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define LOCKNAME_SZ 10

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  char name[LOCKNAME_SZ];
  uint64 size;
  struct spinlock lock;
  struct run *freelist;
} kmem [NCPU];

void
kinit()
{
  for (int i=0; i<NCPU; i++) {
    snprintf(kmem[i].name, LOCKNAME_SZ, "kmem_%d", i);
    kmem[i].size = 0;
    initlock(&kmem[i].lock, kmem[i].name);
  }
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

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  push_off();
  int i = cpuid();
  pop_off();
  acquire(&kmem[i].lock);
  r->next = kmem[i].freelist;
  kmem[i].freelist = r;
  kmem[i].size++;
  release(&kmem[i].lock);
}

void * kalloc_on_cpu(int cpu_id)
{
  struct run *r;
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if(r) {
    kmem[cpu_id].freelist = r->next;
    kmem[cpu_id].size--;
  }
  release(&kmem[cpu_id].lock);
  return (void *)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  void *r;

  push_off();
  int i = cpuid();
  pop_off();

  r = kalloc_on_cpu(i);
  // If no memory available, then we will try to still from whoever has the most
  if(!r) {
    uint64 max_size = 0;
    int target_cpuid = 0;
    for(int j=0;j<NCPU;j++) {
      if (kmem[j].size > max_size) {
        max_size = kmem[j].size;
        target_cpuid = j;
      }
    }
    if (max_size > 0) {
      r = kalloc_on_cpu(target_cpuid);
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
