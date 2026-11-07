// kernel/addrspace.h
// Author: Do Trong Nam - 20233541 (Member A)
// Project 26105: Kernel threads and a user thread library (Xv6-RISCV)
// Shared address-space management, per-thread trapframe slots & reference counting

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_



struct proc;

// Maximum concurrent threads sharing a single address space
#define MAX_THREADS_PER_AS 16

// Per-thread virtual address for trapframe mapping on a shared pagetable.
// Slot 0 maps to standard TRAPFRAME (TRAMPOLINE - PGSIZE).
// Slot k maps to TRAPFRAME - k * PGSIZE, preventing multi-CPU (CPUS=4) collisions.
#define THREAD_TRAPFRAME(slot) (TRAPFRAME - ((uint64)(slot) * PGSIZE))

// Lowest virtual address reserved for thread trapframes (heap ceiling)
#define TRAPFRAME_LOWEST_VA    THREAD_TRAPFRAME(MAX_THREADS_PER_AS - 1)

// Shared address-space descriptor
struct addrspace {
  struct spinlock lock;      // Protects pagetable mutations, sz, and tf_used_bitmap
  pagetable_t pagetable;     // Shared Sv39 user page table
  uint64 sz;                 // Authoritative shared memory size (bytes)
  int refcount;              // Number of live/unreaped threads referencing this space
  uint16 tf_used_bitmap;     // Bitmask of allocated trapframe slots [0..15]
  int valid;                 // Slot in addrspace table is in use
};

// Global lock hierarchy enforced by Member A:
// wait_lock -> p->lock -> as->lock -> kmem.lock

// 1. Address-space lifecycle & reference counting (Member A -> Member B API)
void              as_init(void);
struct addrspace* as_alloc(void);
int               as_retain_for_clone(struct addrspace *as, struct proc *np);
void              as_release(struct proc *p);

// 2. Thread-safe address-space operations (sbrk, fork, exec, user copy)
int               as_grow(struct addrspace *as, int n);
int               as_copy_for_fork(struct addrspace *src_as, struct proc *child_p);
void              as_replace_on_exec(struct proc *p, struct addrspace *new_as);
int               as_copyin(struct addrspace *as, char *dst, uint64 srcva, uint64 len);
int               as_copyout(struct addrspace *as, uint64 dstva, char *src, uint64 len);

// 3. Physical page leak verification helper (1,000-cycle clone/join test)
uint64            kalloc_countfree(void);

#endif // _ADDRSPACE_H_
