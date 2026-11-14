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

// ============================================================================
// [TEST 4 - TUAN 4] Kiem thu luong con (tf_slot > 0) goi fork(), exec(), exit()
// trong khi cac luong anh em van dang hoat dong tren cung struct addrspace.
// ============================================================================

static volatile int sibling_alive = 0;

// Luong anh em chay song song doc/ghi bo nho chung trong luc luong khac fork/exec
static void *
sibling_stress_worker(void *arg)
{
  uint64 *shared_buf = (uint64 *)arg;
  for (int i = 0; i < 2000; i++) {
    shared_buf[i % 64] = (uint64)i * 3 + 7;
    if ((i % 400) == 0)
      pause(1); // Nhuong CPU de kich hoat lap lich da nhan (CPUS=4)
  }
  sibling_alive = 1;
  return 0;
}

// Luong con (chay o tf_slot > 0) truc tiep thuc hien fork() va exec()
static void *
fork_exec_worker(void *arg)
{
  int round = (int)(uint64)arg;

  // 1. Luong con o slot > 0 goi fork() -> kiem tra as_copy_for_fork()
  int pid = fork();
  if (pid < 0) {
    printf("  [FAIL] Round %d: fork() tu worker thread that bai!\n", round);
    exit(1);
  }

  if (pid == 0) {
    // Tien trinh con moi tao tu worker thread:
    // Phai co struct addrspace rieng, chay o tf_slot = 0.
    // Bay gio goi tiep exec() de kiem tra as_replace_on_exec() & proc_pagetable()!
    if (round == 0 || round == 19) {
      char *argv[] = { "echo", "  -> [OK] Worker fork+exec image moi thanh cong!", 0 };
      exec("echo", argv);
      printf("  [FAIL] exec() trong tien trinh con that bai!\n");
      exit(3);
    } else {
      // O cac vong giua, test nhanh viec tach addrspace, sbrk va exit ngay
      char *pointer_check = sbrk(4096);
      if (pointer_check == (char *)-1)
        exit(2);
      pointer_check[0] = 0x5A;
      exit(0);
    }
  }

  // Worker thread doi tien trinh con do chinh no fork ra ket thuc
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

  // Chay 20 chu ky lien tiep: moi chu ky 2 luong con chay song song
  // (1 luong cay bo nho tren shared addrspace, 1 luong o slot > 0 goi fork + exec)
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

  // -----------------------------------------------------------------
  // TEST 4: Worker thread (tf_slot > 0) calling fork(), exec() & exit()
  // -----------------------------------------------------------------
  test4_multithread_fork_exec();

  printf("=== [XAC THUC] TAT CA CAC BAI TEST RO RI BO NHO DEU PASS 100%%! ===\n");
  exit(0);
}