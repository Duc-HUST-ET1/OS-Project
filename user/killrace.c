#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define KILLRACE_ROUNDS 500

static int target_pid;
static int stop_killer;

static void *
probe_worker(void *arg)
{
  return arg;
}

static void *
killer_worker(void *arg)
{
  while (!__atomic_load_n(&stop_killer, __ATOMIC_ACQUIRE)) {
    int pid = __atomic_load_n(&target_pid, __ATOMIC_RELAXED);
    if (pid > 0)
      kill(pid);
    else
      pause(0);
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int tid = thread_create(killer_worker, 0);
  if (tid < 0) {
    printf("killrace: FAIL create killer thread\n");
    exit(1);
  }

  for (int i = 0; i < KILLRACE_ROUNDS; i++) {
    int victim = fork();
    if (victim < 0) {
      printf("killrace: FAIL fork victim at round %d\n", i);
      __atomic_store_n(&stop_killer, 1, __ATOMIC_RELEASE);
      thread_join(tid, 0);
      exit(1);
    }
    if (victim == 0)
      exit(0);

    // Keep the old PID published across wait/reap and the immediately
    // following fork, which encourages the freed addrspace slot to be reused
    // while the killer thread is issuing kill syscalls on another hart.
    __atomic_store_n(&target_pid, victim, __ATOMIC_RELEASE);
    int status = -1;
    if (wait(&status) != victim) {
      printf("killrace: FAIL reap victim at round %d\n", i);
      __atomic_store_n(&stop_killer, 1, __ATOMIC_RELEASE);
      thread_join(tid, 0);
      exit(1);
    }

    int probe = fork();
    if (probe < 0) {
      printf("killrace: FAIL fork reuse probe at round %d\n", i);
      __atomic_store_n(&stop_killer, 1, __ATOMIC_RELEASE);
      thread_join(tid, 0);
      exit(1);
    }
    if (probe == 0) {
      void *result = 0;
      int probe_tid = thread_create(probe_worker, (void *)0x5a);
      if (probe_tid < 0 || thread_join(probe_tid, &result) != probe_tid ||
          result != (void *)0x5a)
        exit(42);
      exit(0);
    }

    status = -1;
    if (wait(&status) != probe || status != 0) {
      printf("killrace: FAIL clone/join after addrspace reuse at round %d\n",
             i);
      __atomic_store_n(&stop_killer, 1, __ATOMIC_RELEASE);
      thread_join(tid, 0);
      exit(1);
    }
  }

  __atomic_store_n(&target_pid, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&stop_killer, 1, __ATOMIC_RELEASE);
  if (thread_join(tid, 0) != tid) {
    printf("killrace: FAIL join killer thread\n");
    exit(1);
  }

  printf("killrace: PASS %d concurrent kill/reap and addrspace-reuse rounds\n",
         KILLRACE_ROUNDS);
  printf("killrace: PASS clone/join in every reused child address space\n");
  printf("killrace: SUMMARY PASS (2 stress checks)\n");
  exit(0);
}
