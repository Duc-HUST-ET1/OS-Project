#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

static void
clonecheck_worker(void)
{
  printf("clonecheck: child ran\n");
  exit(0);
}

int
main(void)
{
  char *stack;
  uint64 child_sp;
  int tid, reaped;

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
  printf("clonecheck: parent got tid %d\n", tid);

  reaped = wait(0);
  if (reaped != tid) {
    printf("clonecheck: FAIL (wait returned %d, expected %d)\n", reaped,
           tid);
    free(stack);
    exit(1);
  }

  free(stack);
  printf("clonecheck: PASS (thread scheduled and reaped)\n");
  exit(0);
}
