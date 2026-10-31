# Test plan index

This repository organizes the project tests according to the development stages described in the assignment.

## Areas

- `functional/` — tests for syscall behavior and argument validation
- `concurrency/` — tests covering shared address space and mutex behavior
- `lifecycle/` — thread creation, termination, join, kill, and wait semantics
- `memory/` — page-table and resource-lifetime checks
- `benchmark/` — parallel sum and stress benchmark planning

## Execution model

Tests are intentionally separated into two categories:

1. guest tests that run inside xv6 user space
2. host-side scripts that orchestrate build and qemu execution

Guest coverage is being added incrementally with the lifecycle implementation.

## Member B clone smoke check

The `clonecheck` guest program exercises the currently registered `clone`
syscall and scheduler path. After booting xv6 with `make qemu`, run `clonecheck`
at the xv6 prompt. A pass prints a child-ran line, the TID returned to the
parent, and `clonecheck: PASS (thread scheduled and reaped)`. It uses `exit`
and `wait` for cleanup; it does not verify the later `thread_exit`/`join`
semantics or group-wide exit policy.

## Requirement checklist

This project must eventually verify:

- each new syscall returns the expected success or failure result
- shared address-space semantics are correct
- `exit`, `kill`, and `wait` interactions are correct for thread groups
- mutex correctness under concurrency
- parallel sum with eight threads
- resource leak detection and lifecycle repetition over ~1000 iterations
- upstream `usertests` and `grind` regressions are still passing
