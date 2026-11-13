#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define ARRAY_SIZE 100000
#define COMPUTE_LOAD 1200

static int array[ARRAY_SIZE];
static long long global_sum = 0;
static struct mutex sum_lock;

struct slice {
  int start;
  int end;
  long long partial_sum;
};

// Hàm tính toán CPU-bound workload
long long heavy_calc(int val) {
  long long res = val;
  for (int i = 0; i < COMPUTE_LOAD; i++) {
    res = (res * 3 + 1) % 1000000007;
  }
  return res;
}

void*
sum_worker(void *arg)
{
  struct slice *s = (struct slice *)arg;
  long long local_sum = 0;

  for (int i = s->start; i < s->end; i++) {
    local_sum += heavy_calc(array[i]);
  }
  s->partial_sum = local_sum;

  mutex_lock(&sum_lock);
  global_sum += local_sum;
  mutex_unlock(&sum_lock);

  return 0;
}

uint run_benchmark(int num_workers, long long *out_sum) {
  mutex_init(&sum_lock);
  global_sum = 0;
  struct slice slices[num_workers];
  int tids[num_workers];
  int chunk = ARRAY_SIZE / num_workers;

  uint start_tick = uptime();
  for (int i = 0; i < num_workers; i++) {
    slices[i].start = i * chunk;
    slices[i].end = (i == num_workers - 1) ? ARRAY_SIZE : (i + 1) * chunk;
    slices[i].partial_sum = 0;
    tids[i] = thread_create(sum_worker, (void *)&slices[i]);
    if (tids[i] < 0) {
      printf("Loi tao luong worker %d\n", i);
      exit(1);
    }
  }

  for (int i = 0; i < num_workers; i++) {
    thread_join(tids[i], 0);
  }
  uint duration = uptime() - start_tick;
  *out_sum = global_sum;
  return duration;
}

int
main(int argc, char *argv[])
{
  printf("============================================================\n");
  printf("  PARALLEL SUM BENCHMARK SUITE - PROJECT 26105\n");
  printf("  Sinh vien thuc hien: Do Phuong Diep - MSSV: 20233315\n");
  printf("============================================================\n\n");

  for (int i = 0; i < ARRAY_SIZE; i++) {
    array[i] = 1;
  }

  // 1. Đo lường đơn luồng tuần tự (Baseline)
  uint t_single_start = uptime();
  long long single_sum = 0;
  for (int i = 0; i < ARRAY_SIZE; i++) {
    single_sum += heavy_calc(array[i]);
  }
  uint t_baseline = uptime() - t_single_start;
  printf("[-] Baseline (Don luong): Tong = %lld | Thoi gian = %d ticks\n\n", single_sum, t_baseline);

  // 2. Chạy lần lượt các cấu hình: 1, 2, 4, 8 workers
  int test_workers[] = {1, 2, 4, 8};
  int num_tests = sizeof(test_workers) / sizeof(test_workers[0]);

  printf("------------------------------------------------------------\n");
  for (int k = 0; k < num_tests; k++) {
    int w = test_workers[k];
    long long multi_sum = 0;
    uint t_multi = run_benchmark(w, &multi_sum);

    printf("-> Workers: %d | Time: %d ticks | Sum: %lld | ", w, t_multi, multi_sum);
    if (multi_sum == single_sum) {
      printf("Status: PASS\n");
    } else {
      printf("Status: FAIL\n");
    }
  }
  printf("------------------------------------------------------------\n");
  printf("Hoan tat benchmark!\n");

  exit(0);
}
