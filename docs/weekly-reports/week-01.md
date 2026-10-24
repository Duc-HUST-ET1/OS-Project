# Week 1 Progress Report — Project 5 (Kernel Threads)

**Course:** Operating Systems - 20261  
**Project:** 05 — Kernel threads and a user thread library  
**Working Period:** 14/10/2026 – 21/10/2026  
**Branch:** `feature/user-thread-mutex`  

## Team Members
- **Member C (Lead/Author):** Do Phuong Diep (Student ID: 20233315) — User thread library & synchronization
- **Member A:** [Họ tên bạn A] (ID: [MSSV A]) — Memory management & reference counting
- **Member B:** [Họ tên bạn B] (ID: [MSSV B]) — Kernel thread lifecycle & syscall semantics

---

## 1. Summary of Latest Progress
During Week 1, the team successfully established the foundational engineering environment for Project 5. All three members cloned the repository, set up individual Git branches according to the contribution policy, and verified that the upstream baseline builds and passes all tests (`make qemu CPUS=1` with `usertests` reporting `ALL TESTS PASSED`). 

The team established the project management board on Trello/GitHub Projects, agreed on branch/review workflows, and divided technical responsibilities. Member C (Do Phuong Diep) analyzed the user-kernel transition mechanisms (trampoline, trap handling, and system calls) and drafted the user-level thread library API and synchronization design.

**Estimated Cumulative Progress:** ~15% (Baseline verified, architecture & data structures mapped).

---

## 2. Review of Technical Activities

### Activity 1: Upstream Baseline & Environment Verification
- Verified RISC-V cross-compilation toolchain and QEMU emulator (`qemu-system-riscv64`).
- Executed `make clean && make qemu` and verified clean test execution (`usertests` passed 100%).

### Activity 2: Codebase Survey (Member C Focus)
- Analyzed `user/usys.pl`, `kernel/trampoline.S`, and `kernel/syscall.c` to trace how system calls are dispatched.
- Identified that syscall numbers 23 (`SYS_hello`) and 24 (`SYS_procinfo`) are already occupied in this tree. Therefore, new syscalls will be assigned:
  - `SYS_clone`: 25
  - `SYS_join`: 26
- Surveyed RISC-V atomic instructions (`amoswap.w.aq`/`rl`) and GCC built-ins (`__atomic_exchange_n`, `__sync_lock_test_and_set`) for implementing user-space mutexes.

### Activity 3: Test Harness & Concurrency Planning
- Designed test cases for race conditions (unprotected vs. mutex-protected shared counters).
- Planned the 8-thread parallel reduction benchmark (`parallel_sum`) comparing execution on `CPUS=1` vs. `CPUS=4`.

---

## 3. Individual Contributions

### Member C: Do Phuong Diep (20233315)
- **Role:** User thread library, synchronization primitives, test orchestration.
- **Accomplishments:**
  - Configured Git identity, checked out and maintained `feature/user-thread-mutex`.
  - Surveyed user memory allocation (`sbrk`/`malloc`) for independent thread user-stack creation.
  - Drafted API prototypes for `user/thread.h`:
    - `int thread_create(void (*fcn)(void*), void *arg);`
    - `int thread_join(void);`
    - `void mutex_init(struct mutex *m);`
    - `void mutex_lock(struct mutex *m);`
    - `void mutex_unlock(struct mutex *m);`
  - Authored test framework plan for multi-threaded workloads.
- **Deliverables / Output:** Updated `docs/weekly-reports/week-01.md`, drafted `user/thread.h` scaffolding, baseline validation log.

### Member A: [Họ tên bạn A - MSSV A]
- **Role:** Memory management & page-table reference counting.
- **Accomplishments:** Analyzed `kernel/vm.c` (`uvmcopy`, `uvmunmap`, `freewalk`) and designed page table sharing rules without premature deallocation.

### Member B: [Họ tên bạn B - MSSV B]
- **Role:** Kernel thread lifecycle & process state.
- **Accomplishments:** Analyzed `kernel/proc.c` (`allocproc`, `fork`, `exit`, `wait`) and mapped thread descriptor fields inside `struct proc`.

---

## 4. Code & Testing Review
- **Algorithmic Development:** Mapped the 1:1 kernel-thread model where child threads share `p->pagetable` while retaining private kernel/user stacks.
- **Testing Results:** Baseline `usertests` passed cleanly on 1 CPU.

---

## 5. Next Period Goals (Week 2 — W2)
1. **Member C (Do Phuong Diep):** Finalize `user/thread.h` and implement the atomic spinlock/mutex using `amoswap`. Prepare stub user programs.
2. **Member A:** Finalize page-table reference counting data structure (`struct pagetable_ref`) and lock hierarchy.
3. **Member B:** Finalize `clone` and `join` kernel signatures, trapframe state transition, and stack alignment rules.
4. **All Members:** Draft the 2–3 page Design Proposal for Week 3 submission.