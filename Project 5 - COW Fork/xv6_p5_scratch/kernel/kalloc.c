// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
  //int ref_count;
};

struct {
  struct spinlock lock;
  struct run *freelist;

  /*
  P5 changes
  */
  uint free_pages; //track free pages
  uint ref_cnt[PHYSTOP / PGSIZE]; //track reference count

} kmem;

extern char end[]; // first address after kernel loaded from ELF file

// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + PGSIZE <= (char*)PHYSTOP; p += PGSIZE){
      kfree(p);
      kmem.ref_cnt[(uint)p/PGSIZE] = 0;
  }
  kmem.free_pages = PHYSTOP / PGSIZE;
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  //memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
  if(kmem.ref_cnt[(uint)v/PGSIZE] > 1){
      kmem.ref_cnt[(uint)v/PGSIZE]--;
  }
  else{
    memset(v, 1, PGSIZE);
    r = (struct run*)v;
    r->next = kmem.freelist;
    kmem.freelist = r;
    kmem.ref_cnt[(uint)v/PGSIZE] = 0;
    kmem.free_pages++;
  }
  
  
  release(&kmem.lock);
}

void
inc_ref(uint v)
{
    acquire(&kmem.lock);
    kmem.ref_cnt[v/PGSIZE]++;
    release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
      kmem.freelist = r->next;
      release(&kmem.lock);
      inc_ref((uint)r);
      kmem.free_pages--;
  }
  else{
    release(&kmem.lock);
  }
  
  return (char*)r;
}

int
getFreePagesCount(void){
    return kmem.free_pages;
}

void dec_ref_count(uint pa) {
    acquire(&kmem.lock);
    if(kmem.ref_cnt[pa/PGSIZE] >= 1){
        kmem.ref_cnt[pa/PGSIZE]--;
    }
    release(&kmem.lock);
}

int
get_ref_count(uint pa){
    return kmem.ref_cnt[pa/PGSIZE];
}
