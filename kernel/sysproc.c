#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"

extern struct proc proc[NPROC];

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_clone(void)
{
  uint64 entry, arg, stack;

  argaddr(0, &entry); // a0: hàm thread_trampoline
  argaddr(1, &arg);   // a1: con trỏ đối số targs
  argaddr(2, &stack); // a2: con trỏ ngăn xếp sp

  // Truyền đúng stack vào vị trí thứ 2, và arg vào vị trí thứ 3 của kthread_create
  return kthread_create(entry, stack, arg);
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  if (t == SBRK_EAGER || n < 0) {
    if (growproc(n, &addr) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if (growproc_lazy(n, &addr) < 0)
      return -1;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (killed(myproc())) {
      release(&tickslock);
      return -1;
    }
    sleep_prepare(&ticks);
    release(&tickslock);
    sleep();
    acquire(&tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
uint64
sys_hello(void)
{
  struct proc *p = myproc();

  printk("kernel: hello() called by pid %d (running in kernel)\n", p->pid);

  return 0;
}
uint64
sys_procinfo(void)
{
  struct proc *p;
  void *mem;

  // Cấp phát một page bộ nhớ trong kernel
  mem = kalloc();

  if (mem == 0) {
    printk("procinfo: kalloc failed\n");
    return -1;
  }

  printk("=== Process Table ===\n");

  for (p = proc; p < &proc[NPROC]; p++) {
    acquire(&p->lock);

    if (p->state != UNUSED) {
      printk("pid=%d state=%d name=%s\n", p->pid, p->state, p->name);
    }

    release(&p->lock);
  }

  // Giải phóng page vừa cấp phát
  kfree(mem);

  return 0;
}
uint64
sys_join(void)
{
  uint64 stack_out;

  argaddr(0, &stack_out);
  return kthread_join(stack_out);
}
