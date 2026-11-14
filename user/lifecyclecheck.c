#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define GROUP_RETVAL 0x123456789abcdef0UL
#define CLONE_ROUNDS 100

static volatile int exec_result = 0;
static volatile int clone_failure = 0;
static volatile int race_start = 0;

static void *
short_worker(void *arg)
{
  pause(2);
  return (void *)(uint64)arg;
}

static void *
blocked_exec_worker(void *arg)
{
  char *argv[] = { "echo", "lifecyclecheck: FAIL exec replaced group", 0 };
  exec_result = exec("echo", argv);
  return (void *)0xabcdef0123456789UL;
}

static void *
fork_wait_worker(void *arg)
{
  int pid = fork();
  if (pid < 0)
    return (void *)-1;
  if (pid == 0) {
    char *argv[] = { "echo", "lifecyclecheck: PASS worker fork child exec", 0 };
    exec("echo", argv);
    exit(31);
  }

  int status = -1;
  if (wait(&status) != pid || status != 0)
    return (void *)-1;
  return (void *)0x55;
}

static void *
clone_churn_worker(void *arg)
{
  while (!race_start)
    pause(1);

  for (int i = 0; i < CLONE_ROUNDS; i++) {
    int tid = thread_create(short_worker, (void *)(uint64)i);
    if (tid < 0 || thread_join(tid, 0) != tid) {
      clone_failure = i + 1;
      return (void *)-1;
    }
  }
  return (void *)CLONE_ROUNDS;
}

static void *
kill_wait_worker(void *arg)
{
  while (pause(1) == 0)
    ;
  return 0;
}

static void
check_kill_group(void)
{
  int child = fork();
  if (child < 0) {
    printf("lifecyclecheck: FAIL fork for kill-group test\n");
    exit(1);
  }
  if (child == 0) {
    if (thread_create(kill_wait_worker, 0) < 0)
      exit(41);
    while (pause(1) == 0)
      ;
    exit(42);
  }

  pause(3); // let child leader and worker reach their sleep points
  if (kill(child) < 0) {
    printf("lifecyclecheck: FAIL kill(process pid)\n");
    exit(1);
  }
  int status = 0;
  if (wait(&status) != child || status != -1) {
    printf("lifecyclecheck: FAIL kill/wait group teardown (status=%d)\n", status);
    exit(1);
  }
  printf("lifecyclecheck: PASS kill(pid) stopped group and wait reaped leader\n");
}

int
main(int argc, char *argv[])
{
  printf("lifecyclecheck: begin policy checks (run with CPUS=4)\n");

  int tid = thread_create(short_worker, (void *)GROUP_RETVAL);
  if (tid < 0) {
    printf("lifecyclecheck: FAIL create worker\n");
    exit(1);
  }
  if (wait(0) != -1) {
    printf("lifecyclecheck: FAIL wait reaped a thread\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS wait does not reap thread contexts\n");

  uint64 retval = 0;
  if (thread_join(tid + 1, 0) != -1 || thread_join(tid, (void **)&retval) != tid ||
      retval != GROUP_RETVAL) {
    printf("lifecyclecheck: FAIL TID-specific join / 64-bit retval\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS exact-TID join returned 0x%lx\n", retval);

  exec_result = 0;
  tid = thread_create(blocked_exec_worker, 0);
  if (tid < 0 || thread_join(tid, 0) != tid || exec_result != -1) {
    printf("lifecyclecheck: FAIL exec policy with live sibling\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS exec rejected while sibling thread exists\n");

  tid = thread_create(fork_wait_worker, 0);
  if (tid < 0) {
    printf("lifecyclecheck: FAIL create fork/wait worker\n");
    exit(1);
  }
  retval = 0;
  if (thread_join(tid, (void **)&retval) != tid || retval != 0x55) {
    printf("lifecyclecheck: FAIL worker fork/wait/exec policy\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS worker forked, waited for, and exec'd child\n");

  char *missing_argv[] = { "missing-executable", 0 };
  if (exec("missing-executable", missing_argv) != -1) {
    printf("lifecyclecheck: FAIL exec of missing file\n");
    exit(1);
  }
  tid = thread_create(short_worker, (void *)7);
  if (tid < 0 || thread_join(tid, 0) != tid) {
    printf("lifecyclecheck: FAIL clone after failed exec\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS failed exec released clone reservation\n");

  tid = thread_create(clone_churn_worker, 0);
  if (tid < 0) {
    printf("lifecyclecheck: FAIL create clone/exec race worker\n");
    exit(1);
  }
  race_start = 1;
  char *exec_argv[] = { "echo", "lifecyclecheck: FAIL concurrent exec committed", 0 };
  if (exec("echo", exec_argv) != -1) {
    // A successful exec replaces this process image, so reaching here is
    // already a policy failure.
    printf("lifecyclecheck: FAIL exec raced with clone publication\n");
    exit(1);
  }
  retval = 0;
  if (thread_join(tid, (void **)&retval) != tid ||
      retval != CLONE_ROUNDS || clone_failure != 0) {
    printf("lifecyclecheck: FAIL clone churn after rejected exec\n");
    exit(1);
  }
  printf("lifecyclecheck: PASS clone/exec exclusion; %d clone/join cycles\n",
         CLONE_ROUNDS);

  check_kill_group();
  printf("lifecyclecheck: SUMMARY PASS (7 policy checks)\n");
  exit(0);
}
