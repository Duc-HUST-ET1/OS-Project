#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

static void
clonecheck_worker(void *arg)
{
  printf("clonecheck: PASS child was scheduled and executed\n");
  thread_exit(0x123456789abcdef0UL);
}

int
main(void)
{
  char *stack;
  uint64 child_sp;
  int tid, reaped;
  uint64 retval = 0;

  stack = malloc(4096);
  if (stack == 0) {
    printf("clonecheck: FAIL (stack allocation)\n");
    exit(1);
  }

  // Leave room below the initial SP for the child's downward-growing stack.
  child_sp = ((uint64)stack + 2048) & ~((uint64)15);
  tid = clone(clonecheck_worker, (void *)child_sp, 0);
  if (tid < 0) {
    printf("clonecheck: FAIL (clone returned %d)\n", tid);
    free(stack);
    exit(1);
  }
  printf("clonecheck: PASS clone created thread (tid=%d)\n", tid);

  retval = 0xfeedfacecafebeefUL;
  if (join(tid + 1, &retval) != -1 || retval != 0xfeedfacecafebeefUL) {
    printf("clonecheck: FAIL wrong TID was accepted or retval was modified\n");
    free(stack);
    exit(1);
  }
  printf("clonecheck: PASS join rejected wrong TID without changing retval\n");

  reaped = join(tid, &retval);
  if (reaped != tid) {
    printf("clonecheck: FAIL exact-TID join returned %d, expected %d\n", reaped,
           tid);
    free(stack);
    exit(1);
  }
  printf("clonecheck: PASS join reaped the requested TID (%d)\n", tid);
  if (retval != 0x123456789abcdef0UL) {
    printf("clonecheck: FAIL expected 0x123456789abcdef0, got 0x%lx\n", retval);
    free(stack);
    exit(1);
  }
  printf("clonecheck: PASS join preserved 64-bit retval 0x%lx\n", retval);
  if (join(tid, 0) != -1) {
    printf("clonecheck: FAIL already-reaped TID was accepted a second time\n");
    free(stack);
    exit(1);
  }
  printf("clonecheck: PASS second join rejected already-reaped TID\n");

  free(stack);
  printf("clonecheck: SUMMARY PASS (6/6 checks)\n");
  exit(0);
}
