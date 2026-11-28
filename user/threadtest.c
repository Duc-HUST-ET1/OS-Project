#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

static struct mutex lock;
static volatile int shared_counter = 0;

void* worker_func(void *arg) {
  int val = (int)(uint64)arg;
  printf("  [Thread] Running with arg = %d\n", val);
  return (void*)(uint64)(val * 2);
}

void* counter_worker(void *arg) {
  for (int i = 0; i < 10000; i++) {
    mutex_lock(&lock);
    shared_counter++;
    mutex_unlock(&lock);
  }
  return 0;
}

void* dummy_worker(void *arg) {
  return 0;
}

int main(int argc, char *argv[]) {
  printf("=== BAT DAU KIEM THU USER THREAD & MUTEX ===\n");
  printf("Sinh vien thuc hien: Do Phuong Diep - MSSV: 20233315\n\n");

  // TEST 1: Tạo luồng và nhận kết quả
  printf("[TEST 1] Kiem tra tao luong va nhan ket qua tra ve:\n");
  int tid1 = thread_create(worker_func, (void*)21);
  void *retval = 0;
  thread_join(tid1, &retval);
  printf("  -> Thread da join xong: TID = %d, Retval = %d\n", tid1, (int)(uint64)retval);
  if ((uint64)retval == 42) {
    printf("[PASS] Test 1 hoan tat!\n\n");
  } else {
    printf("[FAIL] Test 1 that bai!\n\n");
  }

  // TEST 2: Kiểm tra đồng bộ Mutex
  printf("[TEST 2] Kiem tra dong bo Mutex (4 luong x 10000 lan cong):\n");
  mutex_init(&lock);
  shared_counter = 0;
  int tids[4];
  for (int i = 0; i < 4; i++) {
    tids[i] = thread_create(counter_worker, 0);
  }
  for (int i = 0; i < 4; i++) {
    thread_join(tids[i], 0);
  }
  printf("  -> Gia tri cuoi cung cua shared_counter = %d (Ky vong: 40000)\n", shared_counter);
  if (shared_counter == 40000) {
    printf("[PASS] Test 2: Mutex amoswap bao ve tuyet doi chinh xac!\n\n");
  } else {
    printf("[FAIL] Test 2 that bai!\n\n");
  }

  // TEST 3: Stress test 1.000 chu kỳ clone/join (Cổng R3)
  printf("[TEST 3] Stress test tao va thu hoi 1000 chu ky luong lien tiep:\n");
  int stress_ok = 1;
  for (int i = 1; i <= 1000; i++) {
    int tid = thread_create(dummy_worker, 0);
    if (tid < 0) {
      printf("  [FAIL] Khong the tao luong o chu ky %d!\n", i);
      stress_ok = 0;
      break;
    }
    thread_join(tid, 0);
    if (i % 250 == 0) {
      printf("  -> Da hoan tat %d / 1000 chu ky...\n", i);
    }
  }
  if (stress_ok) {
    printf("[PASS] Test 3: Vuot qua 1.000 chu ky, khong ro ri stack/proc!\n\n");
  }

  printf("=== HOAN TAT TAT CA CAC BAI TEST CHUC NANG ===\n");
  exit(0);
}
