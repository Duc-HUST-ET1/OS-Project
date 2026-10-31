// kernel/addrspace.c
// Shared Virtual Address Space & Reference Counting Implementation
// Author: Do Trong Nam - 20233541 (Member A)

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "addrspace.h"

extern struct proc proc[NPROC];

// Global table of shared address spaces
struct addrspace as_table[NPROC];

// Sync cached p->sz across all sibling threads sharing 'as'
// Must be called with as->lock held.
static void
as_sync_proc_sz(struct addrspace *as, uint64 newsz)
{
  for (struct proc *p = proc; p < &proc[NPROC]; p++) {
    if (p->as == as && p->state != UNUSED) {
      p->sz = newsz;
    }
  }
}

// 1. Initialize address-space table at kernel boot
void
as_init(void)
{
  for (int i = 0; i < NPROC; i++) {
    initlock(&as_table[i].lock, "addrspace");
    as_table[i].pagetable = 0;
    as_table[i].sz = 0;
    as_table[i].refcount = 0;
    as_table[i].tf_used_bitmap = 0;
    as_table[i].valid = 0;
  }
}

// Allocate a new shared address space Descriptor (Main Thread gets slot 0)
struct addrspace *
as_alloc(void)
{
  for (int i = 0; i < NPROC; i++) {
    struct addrspace *as = &as_table[i];
    acquire(&as->lock);
    if (!as->valid) {
      as->valid = 1;
      as->pagetable = 0;
      as->sz = 0;
      as->refcount = 1;
      as->tf_used_bitmap = (1U << 0); // Slot 0 reserved for Main Thread
      release(&as->lock);
      return as;
    }
    release(&as->lock);
  }
  return 0;
}

// Retain shared address space for a newly cloned thread and map its private trapframe
int
as_retain_for_clone(struct addrspace *as, struct proc *np)
{
  if (as == 0 || np == 0 || np->trapframe == 0)
    return -1;

  acquire(&as->lock);
  if (!as->valid || as->refcount <= 0 || as->pagetable == 0) {
    release(&as->lock);
    return -1;
  }

  // Find a free trapframe slot in [1 .. MAX_THREADS_PER_AS - 1]
  int slot = -1;
  for (int k = 1; k < MAX_THREADS_PER_AS; k++) {
    if ((as->tf_used_bitmap & (1U << k)) == 0) {
      slot = k;
      break;
    }
  }

  if (slot < 0) {
    release(&as->lock);
    return -1; // Exhausted 16 concurrent threads per address space
  }

  uint64 tf_va = THREAD_TRAPFRAME(slot);

  // Map child thread's physical trapframe at THREAD_TRAPFRAME(slot) in shared pagetable
  if (mappages(as->pagetable, tf_va, PGSIZE,
               (uint64)(np->trapframe), PTE_R | PTE_W) < 0) {
    release(&as->lock);
    return -1;
  }

  as->tf_used_bitmap |= (1U << slot);
  as->refcount++;

  // Attach shared address space metadata to the new thread
  np->as = as;
  np->pagetable = as->pagetable;
  np->sz = as->sz;
  np->tf_slot = slot;
  np->trapframe_va = tf_va;
  np->owns_pagetable = 0;

  asm volatile("sfence.vma zero, zero");
  release(&as->lock);
  return 0;
}

// Release a thread's reference to its shared address space (called in freeproc)
void
as_release(struct proc *p)
{
  if (p == 0 || p->as == 0)
    return;

  struct addrspace *as = p->as;
  int slot = p->tf_slot;

  acquire(&as->lock);

  // Unmap this thread's private trapframe slot from the shared pagetable
  // (do_free = 0 because freeproc() frees p->trapframe physical page separately)
  if (as->pagetable && slot >= 0 && slot < MAX_THREADS_PER_AS) {
    if (as->tf_used_bitmap & (1U << slot)) {
      uvmunmap(as->pagetable, THREAD_TRAPFRAME(slot), 1, 0);
      as->tf_used_bitmap &= ~(1U << slot);
      asm volatile("sfence.vma zero, zero");
    }
  }

  as->refcount--;
  int remaining = as->refcount;

  if (remaining > 0) {
    // Other threads are still alive; keep shared pagetable intact
    release(&as->lock);
  } else {
    // Last reference dropped (refcount == 0): reclaim shared pagetable & physical pages
    pagetable_t pt = as->pagetable;
    uint64 sz = as->sz;

    as->pagetable = 0;
    as->sz = 0;
    as->tf_used_bitmap = 0;
    as->valid = 0;
    release(&as->lock);

    if (pt) {
      // Unmap shared TRAMPOLINE and free all user physical pages [0 .. sz)
      uvmunmap(pt, TRAMPOLINE, 1, 0);
      uvmfree(pt, sz);
    }
  }

  p->as = 0;
  p->pagetable = 0;
  p->owns_pagetable = 0;
  p->sz = 0;
  p->tf_slot = 0;
  p->trapframe_va = 0;
}

// 2. Thread-safe eager sbrk / growproc under as->lock
int
as_grow(struct addrspace *as, int n)
{
  if (as == 0)
    return -1;

  acquire(&as->lock);
  uint64 sz = as->sz;

  if (n > 0) {
    uint64 newsz = sz + (uint64)n;
    if (newsz < sz || newsz > USER_TOP || newsz >= TRAPFRAME_LOWEST_VA) {
      release(&as->lock);
      return -1;
    }
    sz = uvmalloc(as->pagetable, sz, newsz, PTE_W);
    if (sz == 0) {
      release(&as->lock);
      return -1;
    }
  } else if (n < 0) {
    sz = uvmdealloc(as->pagetable, sz, sz + n);
  }

  as->sz = sz;
  as_sync_proc_sz(as, sz);
  release(&as->lock);
  return 0;
}

// Thread-safe address-space deep copy for fork()
int
as_copy_for_fork(struct addrspace *src_as, struct proc *child_p)
{
  if (src_as == 0 || child_p == 0 || child_p->as == 0)
    return -1;

  acquire(&src_as->lock);
  uint64 sz = src_as->sz;
  if (uvmcopy(src_as->pagetable, child_p->as->pagetable, sz) < 0) {
    release(&src_as->lock);
    return -1;
  }
  release(&src_as->lock);

  acquire(&child_p->as->lock);
  child_p->as->sz = sz;
  child_p->sz = sz;
  release(&child_p->as->lock);

  return 0;
}

// Replace current process's address space during exec()
void
as_replace_on_exec(struct proc *p, struct addrspace *new_as)
{
  if (p == 0 || new_as == 0)
    return;

  if (p->as) {
    as_release(p);
  }

  p->as = new_as;
  p->pagetable = new_as->pagetable;
  p->sz = new_as->sz;
  p->tf_slot = 0;
  p->trapframe_va = THREAD_TRAPFRAME(0);
  p->owns_pagetable = 1;
}

// Thread-safe copyin wrapper
int
as_copyin(struct addrspace *as, char *dst, uint64 srcva, uint64 len)
{
  if (as == 0)
    return -1;
  acquire(&as->lock);
  int ret = copyin(as->pagetable, as->sz, dst, srcva, len);
  release(&as->lock);
  return ret;
}

// Thread-safe copyout wrapper
int
as_copyout(struct addrspace *as, uint64 dstva, char *src, uint64 len)
{
  if (as == 0)
    return -1;
  acquire(&as->lock);
  int ret = copyout(as->pagetable, as->sz, dstva, src, len);
  release(&as->lock);
  return ret;
}