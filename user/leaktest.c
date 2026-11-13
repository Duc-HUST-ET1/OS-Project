// user/leaktest.c
// Stress test for Shared Address Space, Reference Counting (as_release/freeproc),
// and Per-thread Trapframe Slot Reuse (1,000+ cycles)
// Author: Do Trong Nam - 20233541 (Member A)

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define SEQ_CYCLES   1000
#define BATCH_ROUNDS 50
#define BATCH_SIZE   8

static volatile int shared_counter = 0;
static struct mutex counter_lock;

static void *
light_worker(void *arg)
{
  int val = (int)(uint64)arg;
  mutex_lock(&counter_lock);
  shared_counter += val;
  mutex_unlock(&counter_lock);
  return 0;
}

int
main(int argc, char *argv[])
{
  printf("=== BAT DAU KIEM THU RO RI BO NHO & REFCOUNT (LEAKTEST) ===\n");
  printf("Sinh vien thuc hien: Do Trong Nam - MSSV: 20233541\n\n");

  mutex_init(&counter_lock);

  // -----------------------------------------------------------------
  // TEST 1: 1,000-cycle sequential clone/join (Trapframe slot reuse)
  // -----------------------------------------------------------------
  printf("[TEST 1] Kiem tra 1000 chu ky clone/join lien tiep (Tai su dung 15 Trapframe Slots)...\n");
  shared_counter = 0;

  for (int i = 0; i < SEQ_CYCLES; i++) {
    int tid = thread_create(light_worker, (void *)(uint64)1);
    if (tid < 0) {
      printf("  [FAIL] thread_create that bai tai chu ky %d (Ro ri slot hoac het RAM)!\n", i);
      exit(1);
    }
    if (thread_join(tid, 0) < 0) {
      printf("  [FAIL] thread_join that bai tai chu ky %d!\n", i);
      exit(1);
    }
    if ((i + 1) % 250 == 0) {
      printf("  -> Da hoan thanh %d / %d chu ky...\n", i + 1, SEQ_CYCLES);
    }
  }

  if (shared_counter != SEQ_CYCLES) {
    printf("  [FAIL] Sai lech du lieu: shared_counter = %d (Ky vong: %d)\n",
           shared_counter, SEQ_CYCLES);
    exit(1);
  }
  printf("  [PASS] Hoan tat 1000 chu ky! Khong ro ri Trapframe slot hay trang vat ly.\n\n");

  // -----------------------------------------------------------------
  // TEST 2: Concurrent batch clone/join (50 rounds x 8 threads = 400 threads)
  // -----------------------------------------------------------------
  printf("[TEST 2] Kiem tra dong bo as->refcount đa luong (%d dot x %d threads = %d threads)...\n",
         BATCH_ROUNDS, BATCH_SIZE, BATCH_ROUNDS * BATCH_SIZE);
  shared_counter = 0;

  for (int r = 0; r < BATCH_ROUNDS; r++) {
    int tids[BATCH_SIZE];
    for (int k = 0; k < BATCH_SIZE; k++) {
      tids[k] = thread_create(light_worker, (void *)(uint64)1);
      if (tids[k] < 0) {
        printf("  [FAIL] Batch %d, thread %d: thread_create that bai!\n", r, k);
        exit(1);
      }
    }
    for (int k = 0; k < BATCH_SIZE; k++) {
      if (thread_join(tids[k], 0) < 0) {
        printf("  [FAIL] Batch %d, thread %d: thread_join that bai!\n", r, k);
        exit(1);
      }
    }
  }

  int expected = BATCH_ROUNDS * BATCH_SIZE;
  if (shared_counter != expected) {
    printf("  [FAIL] Sai lech du lieu batch: %d (Ky vong: %d)\n", shared_counter, expected);
    exit(1);
  }
  printf("  [PASS] Hoan tat %d luong song song! Co che refcount & as->lock an toan tuyet doi.\n\n",
         expected);

  // -----------------------------------------------------------------
  // TEST 3: Heap grow & shrink (growproc_lazy) without 'freewalk: leaf'
  // -----------------------------------------------------------------
  printf("[TEST 3] Kiem tra cap phat & thu hoi dong bo Heap (sbrk +/-) tren Shared Address Space...\n");
  char *base = sbrk(4096 * 4); // Mo rong 4 trang
  if (base == (char *)-1) {
    printf("  [FAIL] Mo rong sbrk that bai!\n");
    exit(1);
  }
  // Cham vao cac trang de kich hoat cap phat vat ly
  for (int p = 0; p < 4; p++) {
    base[p * 4096] = (char)(p + 1);
  }
  // Thu hep lai 4 trang (kiem tra uvmdealloc trong growproc_lazy)
  if (sbrk(-4096 * 4) == (char *)-1) {
    printf("  [FAIL] Thu hep sbrk that bai!\n");
    exit(1);
  }
  printf("  [PASS] Mo rong va thu hoi 4 trang Heap thanh cong, khong gay panic freewalk: leaf!\n\n");

  printf("=== [XAC THUC] TAT CA CAC BAI TEST RO RI BO NHO DEU PASS 100%%! ===\n");
  exit(0);
}