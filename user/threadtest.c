#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define NUM_THREADS 4
#define ITERS_PER_THREAD 10000

volatile int shared_counter = 0;
struct mutex my_lock;

// Worker routine tăng counter có bảo vệ bởi Mutex
void*
counter_worker(void *arg)
{
  for (int i = 0; i < ITERS_PER_THREAD; i++) {
    mutex_lock(&my_lock);
    shared_counter++;
    mutex_unlock(&my_lock);
  }
  return 0;
}

// Routine test truyền đối số và trả về
void*
simple_worker(void *arg)
{
  int val = (int)(uint64)arg;
  printf("  [Thread] Running with arg = %d\n", val);
  return (void *)(uint64)(val * 2);
}

int
main(int argc, char *argv[])
{
  printf("=== BẮT ĐẦU KIỂM THỬ USER THREAD & MUTEX ===\n");
  printf("Sinh viên thực hiện: Do Phuong Diep - MSSV: 20233315\n\n");

  // ---------------------------------------------------------
  // TEST 1: Tạo và Join luồng cơ bản
  // ---------------------------------------------------------
  printf("[TEST 1] Kiểm tra tạo luồng và nhận kết quả trả về:\n");
  int tid1 = thread_create(simple_worker, (void *)21);
  if (tid1 < 0) {
    printf("  -> LỖI: thread_create thất bại!\n");
    exit(1);
  }
  printf("  -> Thread tạo thành công, TID = %d\n", tid1);

  void *retval = 0;
  int reaped = thread_join(tid1, &retval);
  printf("  -> Thread đã join xong: TID = %d, Retval = %d\n", reaped, (int)(uint64)retval);
  if ((int)(uint64)retval == 42) {
    printf("  [PASS] Test 1 hoàn tất!\n\n");
  } else {
    printf("  [FAIL] Test 1: Giá trị trả về không khớp (kỳ vọng 42)!\n\n");
  }

  // ---------------------------------------------------------
  // TEST 2: Kiểm thử tranh chấp vùng nhớ (Race Condition) với Mutex
  // ---------------------------------------------------------
  printf("[TEST 2] Kiểm tra đồng bộ Mutex (%d luồng x %d lần cộng):\n", NUM_THREADS, ITERS_PER_THREAD);
  mutex_init(&my_lock);
  shared_counter = 0;

  int tids[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    tids[i] = thread_create(counter_worker, 0);
    if (tids[i] < 0) {
      printf("  -> LỖI: Không thể tạo luồng %d\n", i);
      exit(1);
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    thread_join(tids[i], 0);
  }

  int expected = NUM_THREADS * ITERS_PER_THREAD;
  printf("  -> Giá trị cuối cùng của shared_counter = %d (Kỳ vọng: %d)\n", shared_counter, expected);
  if (shared_counter == expected) {
    printf("  [PASS] Test 2: Mutex amoswap bảo vệ tuyệt đối chính xác!\n\n");
  } else {
    printf("  [FAIL] Test 2: Xảy ra Race Condition!\n\n");
  }

  printf("=== HOÀN TẤT TẤT CẢ CÁC BÀI TEST CHỨC NĂNG ===\n");
  exit(0);
}
