#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(void)
{
    // Gọi system call hello()
    hello();

    // Sau khi syscall kết thúc, chương trình quay lại user mode
    printf("user: hello() returned, back in user mode\n");

    exit(0);
}
