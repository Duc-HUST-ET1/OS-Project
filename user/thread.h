#ifndef _THREAD_H_
#define _THREAD_H_

#include "kernel/types.h"

// =========================================================================
// USER-LEVEL THREAD LIBRARY & MUTEX INTERFACE
// Author: Do Phuong Diep - Student ID: 20233315 (Member C)
// Project 05: Kernel threads and a user thread library
// =========================================================================

// Cấu trúc khóa người dùng sử dụng lệnh nguyên tử RISC-V (amoswap)
struct mutex {
  uint locked; // 0: UNLOCKED, 1: LOCKED
};

// 1. Quản lý luồng người dùng
int thread_create(void *(*start_routine)(void *), void *arg);
int thread_join(int tid, void **retval);
void thread_exit(void *retval) __attribute__((noreturn));

// 2. Thao tác đồng bộ hóa Mutex
void mutex_init(struct mutex *m);
void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);

#endif // _THREAD_H_
