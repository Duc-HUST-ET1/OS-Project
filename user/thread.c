#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

// Kích thước stack cho mỗi user thread (4KB = 1 page)
#define THREAD_STACK_SIZE 4096
#define MAX_USER_THREADS  64

// Cấu trúc gói tham số truyền cho User Trampoline
struct trampoline_args {
  void *(*routine)(void *);
  void *arg;
};

// Bảng theo dõi stack để giải phóng (tránh Memory Leak khi join)
static struct {
  int tid;
  void *stack_bottom; // Địa chỉ gốc malloc trả về
  void *retval;       // Lưu giá trị trả về
} thread_table[MAX_USER_THREADS];

// Mutex bảo vệ bảng thread nội bộ
static struct mutex table_lock = {0};

// =========================================================================
// 1. ĐỒNG BỘ HÓA MUTEX BẰNG LỆNH NGUYÊN TỬ RISC-V (AMOSWAP)
// =========================================================================

void
mutex_init(struct mutex *m)
{
  m->locked = 0;
}

void
mutex_lock(struct mutex *m)
{
  // Sử dụng __atomic_exchange_n với cờ __ATOMIC_ACQUIRE
  // Trình biên dịch RISC-V GCC sẽ sinh ra đúng lệnh: amoswap.w.aq
  while (__atomic_exchange_n(&m->locked, 1, __ATOMIC_ACQUIRE) != 0) {
    // Nhường CPU trong khi chờ khóa (giảm tải spinlock trên đa nhân)
    pause(0);
  }
}

void
mutex_unlock(struct mutex *m)
{
  // Sử dụng __atomic_store_n với cờ __ATOMIC_RELEASE
  // Đồng nghĩa với việc chèn fence rw,w trước khi ghi 0 (amoswap.w.rl)
  __atomic_store_n(&m->locked, 0, __ATOMIC_RELEASE);
}

// =========================================================================
// 2. USER TRAMPOLINE VÀ QUẢN LÝ VÒNG ĐỜI LUỒNG
// =========================================================================

// Hàm Trampoline bọc ngoài routine: Nhận argument, gọi hàm, tự exit
static void
thread_trampoline(void *arg)
{
  struct trampoline_args *targs = (struct trampoline_args *)arg;
  void *(*routine)(void *) = targs->routine;
  void *real_arg = targs->arg;

  // Thực thi routine chính của luồng
  void *ret = routine(real_arg);

  // Tự động kết thúc luồng với giá trị trả về
  thread_exit(ret);
}

int
thread_create(void *(*start_routine)(void *), void *arg)
{
  // 1. Cấp phát stack trên Heap
  void *stack_bottom = malloc(THREAD_STACK_SIZE);
  if (stack_bottom == 0)
    return -1;

  // 2. Căn chỉnh 16-byte cho RISC-V ABI
  // Stack trong RISC-V mở rộng từ trên xuống (downward growing)
  uint64 sp = (uint64)stack_bottom + THREAD_STACK_SIZE;
  sp = sp & ~((uint64)15);

  // 3. Đặt tham số trampoline lên đỉnh stack
  sp -= sizeof(struct trampoline_args);
  sp = sp & ~((uint64)15); // Tái căn chỉnh 16 bytes
  struct trampoline_args *targs = (struct trampoline_args *)sp;
  targs->routine = start_routine;
  targs->arg = arg;

  // 4. Gọi hàm hệ thống clone
  int tid = clone(thread_trampoline, (void *)targs, (void *)sp);
  if (tid < 0) {
    free(stack_bottom);
    return -1;
  }

  // 5. Lưu thông tin vào bảng theo dõi
  mutex_lock(&table_lock);
  for (int i = 0; i < MAX_USER_THREADS; i++) {
    if (thread_table[i].stack_bottom == 0) {
      thread_table[i].tid = tid;
      thread_table[i].stack_bottom = stack_bottom;
      thread_table[i].retval = 0;
      break;
    }
  }
  mutex_unlock(&table_lock);

  return tid;
}

void
thread_exit(void *retval)
{
  // Gọi trực tiếp exit() với mã trạng thái
  exit((int)(uint64)retval);
}

int
thread_join(int tid, void **retval)
{
  void *joined_stack = 0;
  
  // Gọi hàm hệ thống join(void **stack) của kernel
  int reaped_tid = join(&joined_stack);
  if (reaped_tid < 0)
    return -1;

  // Tìm và giải phóng vùng stack đã cấp phát heap tương ứng
  mutex_lock(&table_lock);
  for (int i = 0; i < MAX_USER_THREADS; i++) {
    // Nếu tid khớp hoặc joined_stack khớp với vùng nhớ của thread
    if (thread_table[i].tid == reaped_tid || 
       (joined_stack >= thread_table[i].stack_bottom && 
        joined_stack <= thread_table[i].stack_bottom + THREAD_STACK_SIZE)) {
      if (retval != 0) {
        *retval = thread_table[i].retval;
      }
      free(thread_table[i].stack_bottom);
      thread_table[i].stack_bottom = 0;
      thread_table[i].tid = 0;
      break;
    }
  }
  mutex_unlock(&table_lock);

  return reaped_tid;
}
