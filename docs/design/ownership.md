# Initial ownership model

This document records the project ownership split for the first implementation cycle.

## Member A — Memory management

Owner of:

- page-table sharing and ownership invariants
- reference counting for shared process resources
- memory leak checks and resource cleanup validation
- review of kernel-thread lifecycle code for lifetime correctness

Primary files to inspect initially:

- `kernel/vm.c`
- `kernel/kalloc.c`
- `kernel/defs.h`
- related memory reclamation code in the kernel

## Member B — Kernel thread lifecycle

Owner of:

- `clone` and `join` design and implementation
- thread metadata, exit handling, and scheduler integration
- process/thread state representation and join semantics
- review of the memory-management implementation

Primary files to inspect initially:

- `kernel/proc.c`
- `kernel/proc.h`
- `kernel/sysproc.c`
- `kernel/syscall.c` and `kernel/syscall.h`

## Member C — User thread library and synchronization

Owner of:

- user-space thread API and synchronization primitives
- appropriate RISC-V atomic operations
- integration between user code and kernel syscalls
- test harness, parallel-sum benchmark, and stress/benchmark automation

Primary files to inspect initially:

- `user/user.h`
- `user/usys.pl`
- `user/ulib.c`
- project-specific tests in `user/` and `tests/`

## Shared review protocol

Before merging any significant implementation:

- one team member writes the proposal and code
- a different member performs the explicit review
- design invariants are checked against the page-table and lifetime rules
- tests are run and recorded before merge

## Initial assignment principles

- keep project ownership explicit and reviewable
- avoid ambiguous file ownership unless the design is agreed before implementation
- track hours and review responsibility in `PROJECT_PLAN.md`
- rebalance work weekly based on actual effort and blockers
