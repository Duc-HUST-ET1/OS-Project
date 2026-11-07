#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define NUM_THREADS 4
#define ITERS_PER_THREAD 10000
#define STRESS_CYCLES 1000

volatile int shared_counter = 0;
struct mutex my_lock;

void* counter_worker(void *arg) {
  for (int i = 0; i < ITERS_PER_THREAD; i++) {
    mutex_lock(&my_lock);
    shared_counter++;
    mutex_unlock(&my_lock);
  }
  return 0;
}

void* simple_worker(void *arg) {
  int val = (int)(uint64)arg;
  return (void *)(uint64)(val * 2);
}

void* dummy_worker(void *arg) {
  return arg;
}

int main(int argc, char *argv[]) {
  printf("=== BAT DAU KIEM THU USER THREAD & MUTEX ===\n");
  printf("Sinh vien thuc hien: Do Phuong Diep - MSSV: 20233315\n\n");

  // TEST 1
  printf("[TEST 1] Kiem tra tao luong va nhan ket qua tra ve:\n");
  int tid1 = thread_create(simple_worker, (void *)21);
  void *retval = 0;
  thread_join(tid1, &retval);
  printf("  -> Thread da join xong: TID = %d, Retval = %d\n", tid1, (int)(uint64)retval);
  if ((int)(uint64)retval == 42) {
    printf("  [PASS] Test 1 hoan tat!\n\n");
  }

  // TEST 2
  printf("[TEST 2] Kiem tra dong bo Mutex (%d luong x %d lan cong):\n", NUM_THREADS, ITERS_PER_THREAD);
  mutex_init(&my_lock);
  shared_counter = 0;
  int tids[NUM_THREADS];
  for (int i = 0; i < NUM_THREADS; i++) {
    tids[i] = thread_create(counter_worker, 0);
  }
  for (int i = 0; i < NUM_THREADS; i++) {
    thread_join(tids[i], 0);
  }
  printf("  -> Gia tri cuoi cung cua shared_counter = %d (Ky vong: %d)\n", shared_counter, NUM_THREADS * ITERS_PER_THREAD);
  if (shared_counter == NUM_THREADS * ITERS_PER_THREAD) {
    printf("  [PASS] Test 2: Mutex amoswap bao ve tuyet doi chinh xac!\n\n");
  }

  // TEST 3 (Cong kiem duyet R3: Stress & Leak test 1.000 chu ky)
  printf("[TEST 3] Stress test tao va thu hoi %d chu ky luong lien tiep:\n", STRESS_CYCLES);
  int stress_pass = 1;
  for (int i = 0; i < STRESS_CYCLES; i++) {
    int tid = thread_create(dummy_worker, (void *)(uint64)i);
    if (tid < 0) {
      printf("  [FAIL] Tran bang tien trinh hoac het RAM o vong %d!\n", i);
      stress_pass = 0;
      break;
    }
    thread_join(tid, 0);
    if ((i + 1) % 250 == 0) {
      printf("  -> Da hoan tat %d / %d chu ky...\n", i + 1, STRESS_CYCLES);
    }
  }
  if (stress_pass) {
    printf("  [PASS] Test 3: Vuot qua 1.000 chu ky, khong ro ri stack/proc!\n\n");
  }

  printf("=== HOAN TAT TAT CA CAC BAI TEST CHUC NANG ===\n");
  exit(0);
}
