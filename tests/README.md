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

The exact guest tests are not yet implemented in this initialization phase; the structure is in place so they can be added without disrupting the upstream tree.

## Requirement checklist

This project must eventually verify:

- each new syscall returns the expected success or failure result
- shared address-space semantics are correct
- `exit`, `kill`, and `wait` interactions are correct for thread groups
- mutex correctness under concurrency
- parallel sum with eight threads
- resource leak detection and lifecycle repetition over ~1000 iterations
- upstream `usertests` and `grind` regressions are still passing
