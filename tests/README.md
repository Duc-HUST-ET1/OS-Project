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

## Member B lifecycle and policy checks

`user/lifecyclecheck.c` exercises the R3/R4 process and thread policies. From
the project root, build and start the guest with four harts:

```sh
make qemu CPUS=4
```

At the xv6 `$` prompt, run the Member B policy check and the required kernel
regression suite. Wait for each command to return to the shell:

```text
lifecyclecheck
usertests -q
```

QEMU builds the kernel and `fs.img` as needed. To exit QEMU from its terminal,
press `Ctrl-A`, then `x`.

For Member B — Trần Anh Đức (MSSV 20233337), the evidence is the seven policy
checks reported by `lifecyclecheck`: `wait` skips threads, exact-TID join
preserves the 64-bit result, exec is rejected with a sibling, a worker can
fork/wait/exec its child, failed exec releases clone admission, clone/exec
exclusion survives 100 clone/join cycles, and process-wide kill is followed by
parent wait/reap. Its final line is
`lifecyclecheck: SUMMARY PASS (7 policy checks)`. The test uses the shared
thread-library interface to start/join worker contexts as test scaffolding;
its PASS assertions report the kernel lifecycle policies. `usertests -q` is
the separate kernel regression check.

Other project binaries include `clonecheck`, `threadtest`, and `leaktest` for
separate smoke or integration coverage. They are not needed for the Member B
evidence listed above. Record the final `usertests -q` result in the weekly
report as the kernel regression check.

## Requirement checklist

This project must eventually verify:

- each new syscall returns the expected success or failure result
- shared address-space semantics are correct
- `exit`, `kill`, `fork`, `exec`, `join`, and `wait` interactions are correct for thread groups
- mutex correctness under concurrency
- parallel sum with eight threads
- resource leak detection and lifecycle repetition over ~1000 iterations
- upstream `usertests` and `grind` regressions are still passing
