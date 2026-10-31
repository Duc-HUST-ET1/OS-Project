#ifndef _THREAD_H_
#define _THREAD_H_

#include "kernel/types.h"

// =========================================================================
// THƯ VIỆN LUỒNG NGƯỜI DÙNG & ĐỒNG BỘ HÓA MUTEX (RISC-V)
// Sinh viên thực hiện: Đỗ Phương Điệp - MSSV: 20233315 (Thành viên C)
// =========================================================================

// Cấu trúc Mutex sử dụng cờ nhị phân
struct mutex {
  uint locked; // 0: UNLOCKED, 1: LOCKED
};

// 1. Thao tác đồng bộ hóa Mutex (Atomic RISC-V amoswap)
void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

// 2. Thao tác quản lý luồng cấp cao
int thread_create(void *(*start_routine)(void *), void *arg);
int thread_join(int tid, void **retval);
void thread_exit(void *retval) __attribute__((noreturn));

#endif // _THREAD_H_
