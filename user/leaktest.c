// user/leaktest.c
// Stress test for Shared Address Space, Reference Counting (as_release/freeproc),
// Per-thread Trapframe Slot Reuse (1,000+ cycles), and Slot Exhaustion Rollback
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

// ============================================================================
// [TEST 4 - TUAN 4] Kiem thu luong con (tf_slot > 0) goi fork(), exec(), exit()
// trong khi cac luong anh em van dang hoat dong tren cung struct addrspace.
// ============================================================================
static volatile int sibling_alive = 0;

static void *
sibling_stress_worker(void *arg)
{
  uint64 *shared_buf = (uint64 *)arg;
  for (int i = 0; i < 2000; i++) {
    shared_buf[i % 64] = (uint64)i * 3 + 7;
    if ((i % 400) == 0)
      pause(1);
  }
  sibling_alive = 1;
  return 0;
}

static void *
fork_exec_worker(void *arg)
{
  int round = (int)(uint64)arg;

  int pid = fork();
  if (pid < 0) {
    printf("  [FAIL] Round %d: fork() tu worker thread that bai!\n", round);
    exit(1);
  }

  if (pid == 0) {
    if (round == 0 || round == 19) {
      char *argv[] = { "echo", "  -> [OK] Worker fork+exec image moi thanh cong!", 0 };
      exec("echo", argv);
      printf("  [FAIL] exec() trong tien trinh con that bai!\n");
      exit(3);
    } else {
      char *pointer_check = sbrk(4096);
      if (pointer_check == (char *)-1)
        exit(2);
      pointer_check[0] = 0x5A;
      exit(0);
    }
  }

  int xstate = -1;
  int wpid = wait(&xstate);
  if (wpid != pid || xstate != 0) {
    printf("  [FAIL] Round %d: wait() loi (wpid=%d, xstate=%d)\n", round, wpid, xstate);
    exit(4);
  }

  return 0;
}

static void
test4_multithread_fork_exec(void)
{
  printf("[TEST 4] Kiem tra luong con (tf_slot > 0) goi fork(), exec() & exit() an toan...\n");

  uint64 *shared_buf = (uint64 *)sbrk(4096);
  if (shared_buf == (uint64 *)-1) {
    printf("  [FAIL] Khong the sbrk bo nho chung cho Test 4!\n");
    exit(1);
  }

  for (int r = 0; r < 20; r++) {
    sibling_alive = 0;

    int t_sib = thread_create(sibling_stress_worker, (void *)shared_buf);
    int t_fe  = thread_create(fork_exec_worker, (void *)(uint64)r);

    if (t_sib < 0 || t_fe < 0) {
      printf("  [FAIL] Round %d: Khong tao duoc worker thread!\n", r);
      exit(1);
    }

    if (thread_join(t_fe, 0) < 0 || thread_join(t_sib, 0) < 0) {
      printf("  [FAIL] Round %d: thread_join that bai trong Test 4!\n", r);
      exit(1);
    }

    if (!sibling_alive) {
      printf("  [FAIL] Round %d: Luong anh em bi anh huong khi luong kia fork/exec!\n", r);
      exit(1);
    }
  }

  sbrk(-4096);
  printf("  [PASS] Hoan tat 20 chu ky worker thread goi fork/exec/exit! Khong crash, khong panic.\n\n");
}

// ============================================================================
// [TEST 5 - TUAN 5] Kiem thu tran Trapframe Slot (MAX_THREADS_PER_AS = 16)
// & Rollback an toan khi clone() that bai, xac minh bang freepages().
// ============================================================================
static volatile int test5_release = 0;

static void *
hold_slot_worker(void *arg)
{
  while (!test5_release) {
    pause(1);
  }
  return 0;
}

static void
test5_slot_exhaustion_and_rollback(void)
{
  printf("[TEST 5] Kiem tra tran Trapframe Slot (15 workers + 3 luong vuot nguong) & Rollback...\n");

  int fp_after_r1 = 0;

  // Chay 2 vong: vong 1 de umalloc cap du pool stack, vong 2 xac minh diff = 0
  for (int round = 1; round <= 2; round++) {
    test5_release = 0;
    int tids[15];
    int created = 0;

    // 1. Lap day 15 slot phu (slot 1..15)
    for (int i = 0; i < 15; i++) {
      tids[i] = thread_create(hold_slot_worker, 0);
      if (tids[i] < 0) {
        printf("  [FAIL] Round %d: Khong tao duoc worker thu %d!\n", round, i + 1);
        test5_release = 1;
        exit(1);
      }
      created++;
    }

    // 2. Co tinh tao 3 luong vuot nguong -> phai that bai (-1) va rollback sach
    for (int extra = 0; extra < 3; extra++) {
      int fail_tid = thread_create(hold_slot_worker, 0);
      if (fail_tid >= 0) {
        printf("  [FAIL] Round %d: Tao luong vuot nguong thu %d lai thanh cong!\n", round, extra + 1);
        test5_release = 1;
        exit(1);
      }
    }

    // 3. Tha 15 luong ket thuc va thu hoi toan bo
    test5_release = 1;
    for (int i = 0; i < created; i++) {
      if (thread_join(tids[i], 0) < 0) {
        printf("  [FAIL] Round %d: thread_join that bai o worker %d!\n", round, i);
        exit(1);
      }
    }

    // 4. Xac minh tai su dung duoc slot ngay lap tuc
    int reuse_tid = thread_create(light_worker, (void *)(uint64)1);
    if (reuse_tid < 0 || thread_join(reuse_tid, 0) < 0) {
      printf("  [FAIL] Round %d: Khong tai su dung duoc slot sau khi tran!\n", round);
      exit(1);
    }

    if (round == 1) {
      fp_after_r1 = (int)freepages();
    } else {
      int fp_after_r2 = (int)freepages();
      printf("  -> Free pages (Test 5 steady-state): r1 = %d, r2 = %d, diff = %d\n",
             fp_after_r1, fp_after_r2, fp_after_r2 - fp_after_r1);
      if (fp_after_r1 != fp_after_r2) {
        printf("  [FAIL] Ro ri trang vat ly sau rollback (%d trang)!\n",
               fp_after_r1 - fp_after_r2);
        exit(1);
      }
    }
  }

  printf("  [PASS] Tran slot & rollback an toan tuyet doi! Khong ro ri RAM hay slot.\n\n");
}
// ============================================================================
// [TEST 6 - TUAN 6] Kiem thu tranh chap dong thoi sbrk() + thoat 8 luong cung luc
// Xac minh as->lock, growproc_lazy va as->refcount-- khong race/leak tren CPUS=4.
// ============================================================================
static void *
concurrent_sbrk_worker(void *arg)
{
  int id = (int)(uint64)arg;
  char *mem = sbrk(4096);
  if (mem == (char *)-1) {
    printf("  [FAIL] Worker %d: sbrk(4096) that bai!\n", id);
    exit(1);
  }

  // Ghi va doc xac thuc de ep page fault (lazy_sbrk) va kiem tra khong de du lieu
  mem[0] = (char)(id & 0xFF);
  mem[4095] = (char)((id ^ 0x5A) & 0xFF);

  if (mem[0] != (char)(id & 0xFF) || mem[4095] != (char)((id ^ 0x5A) & 0xFF)) {
    printf("  [FAIL] Worker %d: Du lieu trang sbrk bi ghi de!\n", id);
    exit(2);
  }
  return 0;
}

static void
test6_concurrent_sbrk_and_exit(void)
{
  printf("[TEST 6] Kiem tra tranh chap sbrk() + thoat 8 luong dong thoi (10 dot = 80 luong)...\n");

  int fp_before = (int)freepages();

  int pid = fork();
  if (pid < 0) {
    printf("  [FAIL] Khong the fork cho Test 6!\n");
    exit(1);
  }
  if (pid == 0) {
    for (int r = 0; r < 10; r++) {
      int tids[8];
      for (int k = 0; k < 8; k++) {
        tids[k] = thread_create(concurrent_sbrk_worker, (void *)(uint64)(r * 8 + k + 1));
        if (tids[k] < 0) {
          printf("  [FAIL] Dot %d: thread_create that bai o luong %d!\n", r, k);
          exit(1);
        }
      }
      for (int k = 0; k < 8; k++) {
        if (thread_join(tids[k], 0) < 0) {
          printf("  [FAIL] Dot %d: thread_join that bai o luong %d!\n", r, k);
          exit(1);
        }
      }
    }
    exit(0);
  }

  int status = -1;
  wait(&status);
  if (status != 0) {
    printf("  [FAIL] Test 6 ket thuc voi ma loi %d!\n", status);
    exit(1);
  }

  int fp_after = (int)freepages();
  printf("  -> Free pages (Test 6): before = %d, after = %d, diff = %d\n",
         fp_before, fp_after, fp_after - fp_before);
  if (fp_before != fp_after) {
    printf("  [FAIL] Ro ri %d trang vat ly sau Test 6!\n", fp_before - fp_after);
    exit(1);
  }

  printf("  [PASS] 80 luong sbrk() & exit() dong thoi an toan tuyet doi! Khong race, khong leak.\n\n");
}

// ============================================================================
// [TEST 7 - TUAN 6] Kiem thu group kill() tren tien trinh da luong dang dung RAM
// Xac minh as_release() thu hoi dung 1 lan khi ca nhom bi ngat giua chung.
// ============================================================================
static void *
kill_target_worker(void *arg)
{
  char *buf = (char *)arg;
  int idx = 0;
  while (1) {
    buf[idx % 4096] = (char)(idx & 0xFF);
    idx += 64;
    if ((idx % 4096) == 0)
      pause(1);
  }
  return 0;
}

static void
test7_group_kill_memory_reclaim(void)
{
  printf("[TEST 7] Kiem tra group kill() tren nhom 6 luong dang dung Heap (15 chu ky)...\n");

  int fp_before = (int)freepages();

  for (int r = 0; r < 15; r++) {
    int pid = fork();
    if (pid < 0) {
      printf("  [FAIL] Round %d: fork that bai!\n", r);
      exit(1);
    }
    if (pid == 0) {
      char *shared_page = sbrk(4096 * 2);
      if (shared_page == (char *)-1)
        exit(1);
      for (int k = 0; k < 6; k++) {
        thread_create(kill_target_worker, (void *)shared_page);
      }
      while (1) {
        pause(2);
      }
    }

    // Cho cac luong con chay thuc su tren CPUS=4 roi kill ca nhom
    pause(3);
    if (kill(pid) < 0) {
      printf("  [FAIL] Round %d: kill(%d) that bai!\n", r, pid);
      exit(1);
    }
    int status = 0;
    if (wait(&status) != pid) {
      printf("  [FAIL] Round %d: wait() khong thu hoi dung leader pid=%d!\n", r, pid);
      exit(1);
    }
  }

  int fp_after = (int)freepages();
  printf("  -> Free pages (Test 7): before = %d, after = %d, diff = %d\n",
         fp_before, fp_after, fp_after - fp_before);
  if (fp_before != fp_after) {
    printf("  [FAIL] Ro ri %d trang vat ly sau group kill()!\n", fp_before - fp_after);
    exit(1);
  }

  printf("  [PASS] 15 chu ky group kill() thu hoi sach 100%% RAM, khong double-free/deadlock!\n\n");
}

static void
run_all_tests(void)
{
  mutex_init(&counter_lock);

  // TEST 1: 1,000-cycle sequential clone/join (Trapframe slot reuse)
  printf("[TEST 1] Kiem tra 1000 chu ky clone/join lien tiep (Tai su dung 15 Trapframe Slots)...\n");
  shared_counter = 0;

  for (int i = 0; i < SEQ_CYCLES; i++) {
    int tid = thread_create(light_worker, (void *)(uint64)1);
    if (tid < 0) {
      printf("  [FAIL] thread_create that bai tai chu ky %d!\n", i);
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

  // TEST 2: Concurrent batch clone/join (50 rounds x 8 threads = 400 threads)
  printf("[TEST 2] Kiem tra dong bo as->refcount da luong (%d dot x %d threads = %d threads)...\n",
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

  // TEST 3: Heap grow & shrink (growproc_lazy) without 'freewalk: leaf'
  printf("[TEST 3] Kiem tra cap phat & thu hoi dong bo Heap (sbrk +/-) tren Shared Address Space...\n");
  char *base = sbrk(4096 * 4);
  if (base == (char *)-1) {
    printf("  [FAIL] Mo rong sbrk that bai!\n");
    exit(1);
  }
  for (int p = 0; p < 4; p++) {
    base[p * 4096] = (char)(p + 1);
  }
  if (sbrk(-4096 * 4) == (char *)-1) {
    printf("  [FAIL] Thu hep sbrk that bai!\n");
    exit(1);
  }
  printf("  [PASS] Mo rong va thu hoi 4 trang Heap thanh cong, khong gay panic freewalk: leaf!\n\n");

  // TEST 4: Worker thread (tf_slot > 0) calling fork(), exec() & exit()
  test4_multithread_fork_exec();

  // TEST 5: Trapframe slot exhaustion & rollback verification
  test5_slot_exhaustion_and_rollback();
  test6_concurrent_sbrk_and_exit();
  test7_group_kill_memory_reclaim();
}

int
main(int argc, char *argv[])
{
  printf("=== BAT DAU KIEM THU RO RI BO NHO & REFCOUNT (LEAKTEST) ===\n");
  printf("Sinh vien thuc hien: Do Trong Nam - MSSV: 20233541\n\n");

  int total_fp_before = (int)freepages();
  printf("[INFO] Free pages truoc toan bo bai test: %d\n\n", total_fp_before);

  int pid = fork();
  if (pid < 0) {
    printf("  [FAIL] Khong the fork tien trinh chay leaktest!\n");
    exit(1);
  }
  if (pid == 0) {
    run_all_tests();
    exit(0);
  }

  int status = -1;
  wait(&status);
  if (status != 0) {
    printf("  [FAIL] Tien trinh kiem thu ket thuc voi ma loi %d!\n", status);
    exit(1);
  }

  int total_fp_after = (int)freepages();
  printf("[SUMMARY] Free pages: before = %d, after = %d, diff = %d\n",
         total_fp_before, total_fp_after, total_fp_after - total_fp_before);
  if (total_fp_before != total_fp_after) {
    printf("  [FAIL] Phat hien ro ri %d trang vat ly!\n",
           total_fp_before - total_fp_after);
    exit(1);
  }

  printf("=== [XAC THUC] TAT CA 7 BAI TEST RO RI BO NHO DEU PASS 100%%! ===\n");
  exit(0);
}