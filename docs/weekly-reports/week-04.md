# Week 4 report

## Goals

- Complete Member B lifecycle policies for R3/R4.
- Exercise multithreaded `kill`, `wait`, `fork`, and `exec` behavior on four CPUs.
- Run the upstream user test suite and record review evidence.

## Status

**Member B — Trần Anh Đức (MSSV 20233337): PASS.** The guest checks below
verify the lifecycle policies assigned to Member B. `lifecyclecheck` uses the
shared user-thread helpers to start and join worker contexts; those helpers are
test scaffolding, and their implementation is not claimed as Member B work.

## Member B work completed

- Added `thread_exit(uint64)` and exact-TID `join(tid, uint64 *)`. Join copies
  the complete 64-bit result before reaping the worker and its trapframe slot.
- Defined `wait` to reap process children only. `kill(pid)` targets all
  contexts with the selected process's `tgid`; process exit shuts down sibling
  threads before the leader becomes waitable.
- Kept `fork` single-threaded and based on the calling context. A worker can
  fork, wait for, and exec its own child.
- Defined `exec` to fail while siblings remain. Added `addrspace.group_lock`
  and an `execing` reservation so clone publication cannot cross exec's
  reference-count check or commit. Group exit uses the same gate to close
  clone admission. The rationale is to prevent a thread from being published
  against an image being replaced or a group being torn down.
- Added `lifecyclecheck`, which prints a separate PASS line for each policy
  outcome. The user-space `thread.c` changes are a syscall adapter needed to
  pass exact TIDs and full-width return values through Member C's API; they do
  not change the mutex implementation.

## Evidence and reproduction steps

From the project root, start QEMU with four harts:

```sh
make qemu CPUS=4
```

At the xv6 `$` prompt, run the Member B policy test, then the required kernel
regression suite. Wait for each command to return to the shell:

```text
lifecyclecheck
usertests -q
```

The verified lifecycle and regression output was:

```text
$ lifecyclecheck
lifecyclecheck: begin policy checks (run with CPUS=4)
lifecyclecheck: PASS wait does not reap thread contexts
lifecyclecheck: PASS exact-TID join returned 0x123456789ABCDEF0
lifecyclecheck: PASS exec rejected while sibling thread exists
lifecyclecheck: PASS worker fork child exec
lifecyclecheck: PASS worker forked, waited for, and exec'd child
lifecyclecheck: PASS failed exec released clone reservation
lifecyclecheck: PASS clone/exec exclusion; 100 clone/join cycles
lifecyclecheck: PASS kill(pid) stopped group and wait reaped leader
lifecyclecheck: SUMMARY PASS (7 policy checks)
$ usertests -q
ALL TESTS PASSED
```

The `lifecyclecheck` results are attributed to Member B's kernel policy work.
Its worker creation and join path uses the shared thread-library interface as
test scaffolding. The test covers the user-visible policy over 100 clone/join
cycles; it does not deterministically inject every instruction-level race.
`usertests -q` is recorded separately as the kernel regression result.

## Policy rationale for R3/R4

- `wait` handles process children and `join` handles thread IDs so legacy
  integer exit status and full-width thread results remain distinct.
- `kill(pid)` is process-wide because leaving an arbitrary subset of a process
  running after termination would leave shared state and resources active.
- `fork` gives the child the calling context's image and parent relationship;
  this lets a worker manage a child it created. The child starts with one
  thread, matching standard process semantics.
- `exec` requires all sibling references to be joined away. `group_lock`
  serializes clone publication with the exec reservation and shutdown gate;
  this closes the check-then-publish race.
- A process leader reaps remaining killed workers before it becomes a zombie,
  keeping the shared page table alive until no worker can execute in it.

## Planned work

- Present the R3/R4 policy and QEMU output at the team review.
- Ask teammates to cross-review the lock order and group shutdown/reap paths.

## Risks and scope

The lifecycle stress test covers the defined user-visible policies and the
regression suite passed on four CPUs. It does not exhaust every possible
instruction-level interleaving. Changes to `user/thread.c` are limited to the
ABI bridge required for the kernel thread-exit and exact-TID join syscalls.
