#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define THREAD_STACK_SIZE 4096
#define MAX_USER_THREADS  64

struct trampoline_args {
  void *(*routine)(void *);
  void *arg;
};

static struct {
  int tid;
  void *stack_bottom;
} thread_table[MAX_USER_THREADS];

static struct mutex table_lock = {0};

void
mutex_init(struct mutex *m)
{
  m->locked = 0;
}

void
mutex_lock(struct mutex *m)
{
  while (__atomic_exchange_n(&m->locked, 1, __ATOMIC_ACQUIRE) != 0) {
    pause(0);
  }
}

void
mutex_unlock(struct mutex *m)
{
  __atomic_store_n(&m->locked, 0, __ATOMIC_RELEASE);
}

static void
thread_trampoline(void)
{
  // Đọc con trỏ tham số trực tiếp từ thanh ghi tp do kernel gán
  register void *tp_reg asm("tp");
  struct trampoline_args *targs = (struct trampoline_args *)tp_reg;

  void *(*routine)(void *) = targs->routine;
  void *real_arg = targs->arg;

  void *ret = routine(real_arg);

  // Gọi trực tiếp syscall thread_exit của Kernel
  thread_exit((uint64)ret);
}

int
thread_create(void *(*start_routine)(void *), void *arg)
{
  void *stack_bottom = malloc(THREAD_STACK_SIZE);
  if (stack_bottom == 0)
    return -1;

  // Tính đỉnh stack căn chỉnh 16 bytes
  uint64 sp = ((uint64)stack_bottom + 2048) & ~((uint64)15);

  sp -= sizeof(struct trampoline_args);
  sp = sp & ~((uint64)15);
  struct trampoline_args *targs = (struct trampoline_args *)sp;
  targs->routine = start_routine;
  targs->arg = arg;

  int tid = clone((void *)thread_trampoline, (void *)sp, (void *)targs);
  if (tid < 0) {
    free(stack_bottom);
    return -1;
  }

  mutex_lock(&table_lock);
  for (int i = 0; i < MAX_USER_THREADS; i++) {
    if (thread_table[i].stack_bottom == 0) {
      thread_table[i].tid = tid;
      thread_table[i].stack_bottom = stack_bottom;
      break;
    }
  }
  mutex_unlock(&table_lock);

  return tid;
}

int
thread_join(int tid, void **retval)
{
  uint64 status = 0;
  // Gọi hàm join chuẩn của Đức: join(tid, &status)
  int reaped_tid = join(tid, &status);
  
  if (reaped_tid > 0) {
    if (retval)
      *retval = (void *)status;

    mutex_lock(&table_lock);
    for (int i = 0; i < MAX_USER_THREADS; i++) {
      if (thread_table[i].tid == reaped_tid || thread_table[i].tid == tid) {
        free(thread_table[i].stack_bottom);
        thread_table[i].stack_bottom = 0;
        thread_table[i].tid = 0;
        break;
      }
    }
    mutex_unlock(&table_lock);
  }

  return reaped_tid;
}
