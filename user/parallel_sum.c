#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/thread.h"

#define ARRAY_SIZE 100000
#define NUM_WORKERS 8

static int array[ARRAY_SIZE];
static long long global_sum = 0;
static struct mutex sum_lock;

struct slice {
  int start;
  int end;
  long long partial_sum;
};

// Worker tính tổng một phần mảng độc lập
void*
sum_worker(void *arg)
{
  struct slice *s = (struct slice *)arg;
  long long local_sum = 0;

  for (int i = s->start; i < s->end; i++) {
    local_sum += array[i];
  }
  s->partial_sum = local_sum;

  // Thu gom kết quả vào biến tổng toàn cục qua Mutex
  mutex_lock(&sum_lock);
  global_sum += local_sum;
  mutex_unlock(&sum_lock);

  return 0;
}

int
main(int argc, char *argv[])
{
  printf("=== BENCHMARK: PARALLEL SUM (8 THREADS) ===\n");
  printf("Sinh viên thực hiện: Do Phuong Diep - MSSV: 20233315\n\n");

  // Khởi tạo mảng: phần tử thứ i có giá trị là 1 -> tổng kỳ vọng là ARRAY_SIZE
  for (int i = 0; i < ARRAY_SIZE; i++) {
    array[i] = 1;
  }

  // ---------------------------------------------------------
  // 1. Chạy ĐƠN LUỒNG (Baseline)
  // ---------------------------------------------------------
  uint start_tick = uptime();
  long long single_sum = 0;
  for (int i = 0; i < ARRAY_SIZE; i++) {
    single_sum += array[i];
  }
  uint single_time = uptime() - start_tick;
  printf("1. Đơn luồng (Single Thread):\n");
  printf("   - Tổng tính được = %lld\n", single_sum);
  printf("   - Thời gian thực thi = %d ticks\n\n", single_time);

  // ---------------------------------------------------------
  // 2. Chạy ĐA LUỒNG (8 Threads song song)
  // ---------------------------------------------------------
  mutex_init(&sum_lock);
  global_sum = 0;
  struct slice slices[NUM_WORKERS];
  int tids[NUM_WORKERS];
  int chunk = ARRAY_SIZE / NUM_WORKERS;

  start_tick = uptime();
  for (int i = 0; i < NUM_WORKERS; i++) {
    slices[i].start = i * chunk;
    slices[i].end = (i == NUM_WORKERS - 1) ? ARRAY_SIZE : (i + 1) * chunk;
    slices[i].partial_sum = 0;

    tids[i] = thread_create(sum_worker, (void *)&slices[i]);
    if (tids[i] < 0) {
      printf("Lỗi tạo luồng số %d!\n", i);
      exit(1);
    }
  }

  for (int i = 0; i < NUM_WORKERS; i++) {
    thread_join(tids[i], 0);
  }
  uint multi_time = uptime() - start_tick;

  printf("2. Đa luồng song song (%d Workers):\n", NUM_WORKERS);
  printf("   - Tổng tính được = %lld\n", global_sum);
  printf("   - Thời gian thực thi = %d ticks\n\n", multi_time);

  if (global_sum == single_sum) {
    printf("[XÁC THỰC] Kết quả 2 phương pháp hoàn toàn trùng khớp!\n");
  } else {
    printf("[CẢNH BÁO] Kết quả tính toán không khớp!\n");
  }

  exit(0);
}
