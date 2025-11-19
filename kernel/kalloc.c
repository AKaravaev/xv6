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
kfree_on_cpu(void *pa, int cpu_id)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  kmem[cpu_id].size++;
  release(&kmem[cpu_id].lock);
}

void
freerange_on_cpu(void *pa_start, void *pa_end, int cpu_id)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree_on_cpu(p, cpu_id);
}

void
kinit()
{
  for (int i=0; i<NCPU; i++) {
    snprintf(kmem[i].name, LOCKNAME_SZ, "kmem_%d", i);
    initlock(&kmem[i].lock, kmem[i].name);
  }
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  push_off();
  int i = cpuid();
  pop_off();
  freerange_on_cpu(pa_start, pa_end, i);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  push_off();
  int i = cpuid();
  pop_off();
  kfree_on_cpu(pa, i);
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

uint64 ksteal(int cpu_id, struct run **p) {
  struct run *r;
  uint64 sz;

  acquire(&kmem[cpu_id].lock);
  if (!kmem[cpu_id].size) {
    release(&kmem[cpu_id].lock);
    return 0;
  }

  // Stealing one page only, but could make it variable
  // depending on the size of the available queue
  sz = 1;
  r = kmem[cpu_id].freelist;

  for(uint64 i=0; i < sz-1; i++)
    r = r->next;

  *p = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r->next;
  r->next = 0;
  kmem[cpu_id].size -= sz;
  release(&kmem[cpu_id].lock);
  return sz;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run **pr;
  struct run *r;
  int i,j;
  uint64 sz;

  push_off();
  i = cpuid();
  pop_off();

  r = kalloc_on_cpu(i);
  // If no memory available, then we will try to steal from the proc with max memory
  uint64 max_size = 0;
  int from_cpu = 0;
  for(j=0;j<NCPU && !r;j++) {
    if ((j == i) || !kmem[j].freelist) continue;
    if (kmem[j].size > max_size) {
      max_size = kmem[j].size;
      from_cpu = j;
    }
  }
  if (max_size > 0) {
    sz = ksteal(from_cpu, &r);
    if (sz) {
      acquire(&kmem[i].lock);
      for (pr=&kmem[i].freelist; *pr ; pr=&((*pr)->next));
      *pr = r->next;
      kmem[i].size += sz-1;
      release(&kmem[i].lock);
    }
  }
  if(r) {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
  return (void*)r;
}
