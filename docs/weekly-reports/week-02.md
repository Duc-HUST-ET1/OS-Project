# Week 2 report

## Goals

- finalize the design proposal
- document memory ownership and page-table lifetime invariants
- review the process and syscall interfaces that will be affected

## Status

Member B implemented the B1–B3 kernel lifecycle foundation in this worktree.
No performance measurements are reported. Syscall registration and the
user-facing thread library remain later integration work in the plan.

## Completed work (Member B)

- Added per-context group metadata (`tgid`, thread marker, and 64-bit exit
  result) with allocation and cleanup initialization.
- Added a kernel `kthread_create(entry, stack, tls)` path. It validates an
  executable user entry and writable user stack, allocates the proc/trapframe,
  attaches the child to the caller's page table, initializes `epc`, `sp`, `tp`,
  and `a0`, duplicates file and cwd references, records the parent, and only
  then publishes `RUNNABLE` under the child proc lock.
- Reserved a distinct trapframe VA for each proc slot. `prepare_return()` puts
  that VA in `sscratch`; the trampoline uses it both on trap entry and return.
- Serialized shared-page-table growth and child trapframe mapping with
  `thread_vm_lock`; documented lifecycle lock order and R2 evidence.

## Decisions and rationale

- The earlier `clone(flags, stack, tls)` sketch did not say how the kernel
  would learn the fixed user trampoline address. Member B chose the explicit
  `clone(entry, stack, tls)` contract, and the kernel primitive uses the same
  argument order. The user library can pass `thread_trampoline` as `entry`
  without overloading `flags` with a function pointer.
- `stack` means the aligned initial stack pointer pointing at the startup
  record, rather than an exclusive stack boundary. That matches the proposed
  trampoline behavior, which reads the startup record from `sp`.
- A single fixed trapframe VA is unsafe for simultaneous contexts sharing a
  page table. Each context therefore uses its proc-slot VA; `sscratch` carries
  the active VA across the privilege transition.
- `a0` is initialized to zero per the syscall child-return convention; `tp`
  receives TLS as requested by the interface sketch. The kernel leaves startup
  record interpretation to the user trampoline.
- Scheduler publication is the final operation after validation, memory
  mapping, register initialization, resource references, and parent linkage.
  Failures before that point discard the unpublished child.

## Scope boundary

The follow-on B5 integration registers `clone` as syscall 25 and adds its
dispatch handler, user prototype, and generated assembly stub. `join` and
`thread_exit` registration depends on B4 lifecycle handlers and is still
pending. Group-wide exit, concurrent `exec`, process-wide kill, and shared
page-table final-release policy remain B4/B6 integration risks and must be
completed before relying on full thread-group lifecycle semantics.

For direct evidence, `make fs.img` builds the guest `clonecheck` program and
includes it in the xv6 image. In QEMU (`make qemu CPUS=1`), running `clonecheck`
prints a PASS line for each checked behavior and finishes with
`clonecheck: SUMMARY PASS (6/6 checks)`. The checks are thread creation,
scheduling/execution, rejection of a wrong TID without modifying the output,
exact-TID join/reap, preservation of a 64-bit retval, and rejection of a
second join after reap.

## Risks

- group-wide exit and final page-table release are not implemented in B1–B3
- concurrent file-table changes and `exec` need group-level integration
- inadequate cross-review of kernel lifecycle code

## Task 2 progress (Member B; R3/R4 preparation)

- Added `thread_exit(uint64 retval)` and `join(tid, uint64 *retval)` syscalls.
  A thread records its full 64-bit result when it becomes a zombie. `join`
  accepts only the exact TID in the caller's thread group, copies the result
  out before reaping, and releases that thread's trapframe mapping.
- Updated the `clonecheck` guest program to verify a nontrivial 64-bit return
  value (`0x123456789abcdef0`), reject a wrong TID, and reject a second reap.
  Build and QEMU output:
  `clonecheck: SUMMARY PASS (6/6 checks)` with each passed behavior printed
  separately.
- `fork` now serializes the address-space copy with shared-page-table growth.
  A process child is assigned to the thread-group leader so the leader can
  reap it with `wait`.
- `wait` ignores thread contexts; those are reaped by `join`. `kill(pid)`
  treats the PID as a process-group target and marks every context with that
  `tgid` as killed. Killed worker threads take the thread-exit path.
- `exec` returns `-1` while any sibling context remains allocated. The caller
  must join siblings first, avoiding replacement of a page table still used
  by another thread.

### Policy decisions and rationale

- **Join is TID-specific and group-scoped.** A caller cannot reap another
  process's thread or accidentally consume a different worker's result. The
  result uses `uint64` end to end to preserve pointers and other RV64 values.
- **`wait` is for child processes; `join` is for threads.** This keeps the
  legacy `wait(int *)` exit-status contract intact while giving threads a
  separate full-width result contract.
- **`kill(pid)` is process-wide.** A process cannot safely continue with only
  an arbitrary subset of its threads after external termination. The supplied
  PID identifies the group through `tgid`.
- **`fork` creates a single-threaded child from the calling context's memory
  image.** The copied register state belongs to the caller, and the new child
  starts its own group; serializing the copy prevents overlap with `sbrk`.
- **`exec` requires siblings to be joined first.** Rejecting it is a small,
  explicit policy that avoids invalidating a shared page table underneath
  running threads.

When a process leader exits, it now kills and reaps its remaining worker
contexts before becoming a waitable zombie, so the shared page table stays
valid. A worker that invokes the legacy process `exit()` syscall itself is
still outside the supported user-thread contract; user workers must use
`thread_exit`. Atomic exclusion between a concurrent `clone` and `exec` also
needs a group lifecycle lock/API. The current `exec` sibling scan enforces the
policy in the ordinary serialized path, but is not a complete concurrency
barrier. The QEMU check covers thread return and reap; it does not claim
coverage of group-wide races or the kill/wait/fork/exec matrix.
