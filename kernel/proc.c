#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "addrspace.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void
forkret(void);
static void
freeproc(struct proc *p);
static struct proc *
allocproc(void);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;


// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void
proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void
procinit(void)
{
  struct proc *p;

  as_init();
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++) {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int
cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

// Allocate a schedulable context that shares the caller's user address space.
// entry is a user executable address; stack points at the user startup record.
// The child is published RUNNABLE only after its trapframe and metadata are
// complete and its private trapframe mapping exists in the shared page table.
int
kthread_create(uint64 entry, uint64 stack, uint64 tls)
{
  struct proc *parent = myproc();
  struct proc *np;
  struct addrspace *as = parent->as;
  pte_t *pte;
  int fd, tid;

  if (as == 0)
    return -1;

  if ((np = allocproc()) == 0)
    return -1;

  // Drop the private address space allocated by allocproc before attaching
  // the child thread to the parent's shared address space.
  as_release(np);
  release(&np->lock);

  // Hold the group gate until the new thread is fully published. An exec
  // reservation therefore either sees this thread or prevents its creation.
  acquire(&as->group_lock);
  if (parent->as != as || as->execing || as->exiting)
    goto fail_group;

  if ((stack & 15) != 0 || parent->sz < 2 * sizeof(uint64) ||
      stack > parent->sz - 2 * sizeof(uint64) || entry >= parent->sz)
    goto fail_group;
  pte = walk(parent->pagetable, entry, 0);
  if (pte == 0 || (*pte & (PTE_V | PTE_U | PTE_X)) !=
                      (PTE_V | PTE_U | PTE_X))
    goto fail_group;
  pte = walk(parent->pagetable, stack, 0);
  if (pte == 0 || (*pte & (PTE_V | PTE_U | PTE_W)) !=
                      (PTE_V | PTE_U | PTE_W))
    goto fail_group;

  acquire(&parent->as->lock);
  // Revalidate while page-table changes are serialized under parent->as->lock
  if ((stack & 15) != 0 || parent->as->sz < 2 * sizeof(uint64) ||
      stack > parent->as->sz - 2 * sizeof(uint64) || entry >= parent->as->sz) {
    release(&parent->as->lock);
    goto fail_group;
  }
  pte = walk(parent->as->pagetable, entry, 0);
  if (pte == 0 || (*pte & (PTE_V | PTE_U | PTE_X)) !=
                      (PTE_V | PTE_U | PTE_X)) {
    release(&parent->as->lock);
    goto fail_group;
  }
  pte = walk(parent->as->pagetable, stack, 0);
  if (pte == 0 || (*pte & (PTE_V | PTE_U | PTE_W)) !=
                      (PTE_V | PTE_U | PTE_W)) {
    release(&parent->as->lock);
    goto fail_group;
  }
  release(&parent->as->lock);

  // Retain shared address space and map private trapframe slot [1..15]
  if (as_retain_for_clone(as, np) < 0) {
    goto fail_group;
  }

  np->is_thread = 1;
  np->tgid = parent->tgid;
  np->trapframe->epc = entry;
  np->trapframe->sp = stack;
  np->trapframe->tp = tls;
  np->trapframe->a0 = tls;
  safestrcpy(np->name, parent->name, sizeof(np->name));
  tid = np->pid;

  for (fd = 0; fd < NOFILE; fd++)
    if (parent->ofile[fd])
      np->ofile[fd] = filedup(parent->ofile[fd]);
  np->cwd = idup(parent->cwd);

  acquire(&wait_lock);
  acquire(&np->lock);
  np->parent = parent;
  np->state = RUNNABLE;
  release(&np->lock);
  release(&wait_lock);
  release(&as->group_lock);
  return tid;

fail_group:
  release(&as->group_lock);
  acquire(&np->lock);
  freeproc(np);
  release(&np->lock);
  return -1;
}

void
kthread_exit(uint64 retval)
{
  struct proc *p = myproc();
  struct proc *leader = 0;

  if (!p->is_thread)
    kexit((int)retval);

  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      fileclose(p->ofile[fd]);
      p->ofile[fd] = 0;
    }
  }
  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);
  for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
    if (tp->pid == p->tgid && !tp->is_thread && tp->state != UNUSED) {
      leader = tp;
      break;
    }
  }
  wakeup(p->parent);
  if (leader && leader != p)
    wakeup(leader);
  acquire(&p->lock);
  p->xstate64 = retval;
  p->xstate = (int)retval;
  p->state = ZOMBIE;
  release(&wait_lock);
  sched();
  panic("thread zombie exit");
}

// Join one exact TID in the caller's process group. Copy the full-width
// result before releasing the thread's address-space reference and slot.
int
kthread_join(int tid, uint64 retval_addr)
{
  struct proc *np;
  struct proc *p = myproc();
  uint64 retval;
  int found;

  if (tid <= 0 || p->as == 0)
    return -1;

  acquire(&wait_lock);

  for (;;) {
    found = 0;
    for (np = proc; np < &proc[NPROC]; np++) {
      acquire(&np->lock);
      if (np->pid == tid && np->tgid == p->tgid && np->is_thread &&
          np != p) {
        found = 1;
        if (np->state == ZOMBIE) {
          retval = np->xstate64;
          if (retval_addr != 0 &&
              as_copyout(p->as, retval_addr, (char *)&retval,
                         sizeof(retval)) < 0) {
            release(&np->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(np);
          release(&np->lock);
          release(&wait_lock);
          return tid;
        }
      }
      release(&np->lock);
    }

    if (!found || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    sleep_prepare(p);
    release(&wait_lock);
    sleep();
    acquire(&wait_lock);
  }
}

int
allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state == UNUSED) {
      goto found;
    } else {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->tgid = p->pid;
  p->is_thread = 0;
  p->xstate64 = 0;
  p->owns_pagetable = 1;
  p->tf_slot = 0;
  p->trapframe_va = THREAD_TRAPFRAME(0);
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Allocate a shared address-space descriptor.
  if ((p->as = as_alloc()) == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table with TRAMPOLINE and slot-0 TRAPFRAME mapped.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0) {
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  p->as->pagetable = p->pagetable;

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

  return p;
}

static void
freeproc(struct proc *p)
{
  if (p->as) {
    as_release(p);
  } else if (p->pagetable && p->owns_pagetable) {
    proc_freepagetable(p->pagetable, p->sz, p->trapframe_va);
  }
  p->pagetable = 0;
  p->owns_pagetable = 0;
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  p->sz = 0;
  p->pid = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->xstate64 = 0;
  p->tgid = 0;
  p->is_thread = 0;
  p->tf_slot = 0;
  p->trapframe_va = 0;
  p->state = UNUSED;
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE, (uint64)trampoline,
               PTE_R | PTE_X) < 0) {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, p->trapframe_va, PGSIZE, (uint64)(p->trapframe),
               PTE_R | PTE_W) < 0) {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void
proc_freepagetable(pagetable_t pagetable, uint64 sz, uint64 trapframe_va)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, trapframe_va, 1, 0);
  uvmfree(pagetable, sz);
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n, uint64 *oldsz)
{
  struct proc *p = myproc();
  if (p->as == 0)
    return -1;

  acquire(&p->as->lock);
  *oldsz = p->as->sz;
  release(&p->as->lock);

  return as_grow(p->as, n);
}

int
growproc_lazy(int n, uint64 *oldsz)
{
  struct proc *p = myproc();
  if (p->as == 0)
    return -1;

  acquire(&p->as->lock);
  uint64 sz = p->as->sz;
  *oldsz = sz;

  uint64 newsz;
  if (n > 0) {
    newsz = sz + (uint64)n;
    // Cho phép chạm mốc TRAPFRAME, nhưng vượt quá TRAPFRAME (như sbrklazy(1)) thì báo lỗi -1
    if (newsz < sz || newsz > TRAPFRAME) {
      release(&p->as->lock);
      return -1;
    }
  } else if (n < 0) {
    if ((uint64)(-n) > sz) {
      release(&p->as->lock);
      return -1;
    }
    newsz = uvmdealloc(p->as->pagetable, sz, sz - (uint64)(-n));
  } else {
    newsz = sz;
  }

  p->as->sz = newsz;
  for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
    if (tp->as == p->as && tp->state != UNUSED) {
      tp->sz = newsz;
    }
  }
  release(&p->as->lock);
  return 0;
}

int
kfork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0) {
    return -1;
  }

  // Copy user memory from parent's shared address space to child.
  if (as_copy_for_fork(p->as, np) < 0) {
    freeproc(np);
    release(&np->lock);
    return -1;
  }

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++) {
    if (pp->parent == p) {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Close clone admission first, then request termination for every schedulable
// context in the target process group. group_lock makes this atomic with the
// clone publication path.
static void
request_group_exit(int tgid, struct addrspace *as)
{
  if (as) {
    acquire(&as->group_lock);
    as->exiting = 1;
    as->execing = 0;
    release(&as->group_lock);
  }

  for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
    acquire(&tp->lock);
    if (tp->state != UNUSED && tp->tgid == tgid &&
        tp->state != ZOMBIE) {
      tp->killed = 1;
      if (tp->state == SLEEPING)
        tp->state = RUNNABLE;
    }
    release(&tp->lock);
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
kexit(int status)
{
  struct proc *p = myproc();
  struct addrspace *as = p->as;
  struct proc *leader = 0;

  if (p == initproc)
    panic("init exiting");

  request_group_exit(p->tgid, as);

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++) {
    if (p->ofile[fd]) {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

  acquire(&wait_lock);

  // The leader owns process-level wait/reap. Keep the shared address space
  // alive until all worker contexts have reached ZOMBIE, then reclaim them.
  if (!p->is_thread) {
    int active;
    do {
      active = 0;
      for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
        if (tp == p)
          continue;
        acquire(&tp->lock);
        if (tp->tgid == p->tgid && tp->is_thread &&
            tp->state != UNUSED && tp->state != ZOMBIE) {
          tp->killed = 1;
          if (tp->state == SLEEPING)
            tp->state = RUNNABLE;
          active = 1;
        }
        release(&tp->lock);
      }
      if (active) {
        sleep_prepare(p);
        release(&wait_lock);
        sleep();
        acquire(&wait_lock);
      }
    } while (active);

    for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
      if (tp == p)
        continue;
      acquire(&tp->lock);
      if (tp->tgid == p->tgid && tp->is_thread && tp->state == ZOMBIE) {
        tp->parent = 0;
        freeproc(tp);
      }
      release(&tp->lock);
    }
  }

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  // A thread can be the creator of the group's worker that the leader is
  // waiting for. Notify the leader explicitly after publishing ZOMBIE.
  leader = 0;
  for (struct proc *tp = proc; tp < &proc[NPROC]; tp++) {
    if (tp->pid == p->tgid && !tp->is_thread && tp->state != UNUSED) {
      leader = tp;
      break;
    }
  }
  if (leader && leader != p) {
    release(&p->lock);
    wakeup(leader);
    acquire(&p->lock);
  }

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
kwait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;) {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++) {
      if (pp->parent == p && !pp->is_thread) {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE) {
          // Found one.
          pid = pp->pid;
          if (addr != 0 &&
              copyout(p->pagetable, p->sz, addr, (char *)&pp->xstate,
                      sizeof(pp->xstate)) < 0) {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          pp->parent = 0;
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p)) {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep_prepare(p); //DOC: wait-sleep
    release(&wait_lock);
    sleep();
    acquire(&wait_lock);
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;) {
    // The most recent process to run may have had interrupts
    // turned off; enable them to avoid a deadlock if all
    // processes are waiting. Then turn them back off
    // to avoid a possible race between an interrupt
    // and wfi.
    intr_on();
    intr_off();

    int found = 0;
    for (p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if (p->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);

        // Don't re-enable interrupts on release.
        mycpu()->intena = 0;

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
        found = 1;
      }
      release(&p->lock);
    }
    if (found == 0) {
      // nothing to run; stop running on this core until an interrupt.
      asm volatile("wfi");
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched RUNNING");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void
forkret(void)
{
  extern char userret[];
  static int first = 1;
  struct proc *p = myproc();

  // Still holding p->lock from scheduler.
  release(&p->lock);

  if (first) {
    first = 0;

    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    fsinit(ROOTDEV);

    // We can invoke kexec() now that file system is initialized.
    // Put the return value (argc) of kexec into a0.
    p->trapframe->a0 = kexec("/init", (char *[]){"/init", 0});
    if (p->trapframe->a0 == -1) {
      panic("exec");
    }
  }

  // return to user space, mimicing usertrap()'s return.
  prepare_return();
  uint64 satp = MAKE_SATP(p->pagetable);
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// Register current process as waiting for wakeups on chan.
void
sleep_prepare(void *chan)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  if (chan == 0)
    panic("sleep_prepare: zero chan");
  p->chan = chan;
  release(&p->lock);
}

// Put the thread to sleep.  Assumes sleep_prepare() was called before.
// If the channel registered by sleep_prepare() has been woken up in
// the meantime, do not go to sleep, and instead return immediately.
void
sleep(void)
{
  struct proc *p = myproc();

  acquire(&p->lock);
  if (p->chan != 0) {
    p->state = SLEEPING;
    sched();
  }
  release(&p->lock);
}

// Wake up all processes sleeping on channel chan.
void
wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->chan == chan) {
      // If the process is waiting for wakeups on this channel,
      // signal that the wakeup happened by clearing p->chan.
      p->chan = 0;

      // If this waiting process has gotten so far as to actually
      // go to sleep, also set it back to RUNNING.
      if (p->state == SLEEPING) {
        p->state = RUNNABLE;
      }
    }
    release(&p->lock);
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int
kkill(int pid)
{
  struct proc *p;
  struct addrspace *as = 0;
  int tgid = -1;
  int valid = 0;

  if (pid == 0)
    return -1;

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->pid == pid && p->state != UNUSED) {
      tgid = p->tgid;
      as = p->as;
      release(&p->lock);
      break;
    }
    release(&p->lock);
  }
  if (tgid < 0)
    return -1;

  // The first lookup only discovers the address-space descriptor. It may be
  // reaped before we acquire its group gate, so revalidate the exact pid,
  // group, and descriptor while holding group_lock and the proc lock. This
  // also keeps a reused addrspace slot from receiving a stale kill request.
  acquire(&as->group_lock);
  acquire(&p->lock);
  if (p->pid == pid && p->tgid == tgid && p->as == as &&
      p->state != UNUSED) {
    acquire(&as->lock);
    if (as->valid && as->refcount > 0) {
      as->exiting = 1;
      as->execing = 0;
      valid = 1;
    }
    release(&as->lock);
  }
  release(&p->lock);
  release(&as->group_lock);

  if (!valid)
    return -1;

  // Do not retain the descriptor past the gate: the group may finish exit
  // and release its address space while this scan marks its contexts killed.
  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);
    if (p->state != UNUSED && p->tgid == tgid && p->state != ZOMBIE) {
      p->killed = 1;
      if (p->state == SLEEPING)
        p->state = RUNNABLE;
    }
    release(&p->lock);
  }
  return 0;
}

void
setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int
killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int
either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst) {
    return copyout(p->pagetable, p->sz, dst, src, len);
  } else {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int
either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src) {
    return copyin(p->pagetable, p->sz, dst, src, len);
  } else {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
    // clang-format off
    [UNUSED]    = "unused",
    [USED]      = "used",
    [SLEEPING]  = "sleep ",
    [RUNNABLE]  = "runble",
    [RUNNING]   = "run   ",
    [ZOMBIE]    = "zombie"
    // clang-format on
  };
  struct proc *p;
  char *state;

  printk("\n");
  for (p = proc; p < &proc[NPROC]; p++) {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printk("%d %s %s", p->pid, state, p->name);
    printk("\n");
  }
}
