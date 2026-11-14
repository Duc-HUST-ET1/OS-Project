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

## Task 2 progress (Member B; R3/R4 completed)

- Added `thread_exit(uint64 retval)` and `join(tid, uint64 *retval)`. Join is
  scoped to an exact TID in the caller's group, copies all 64 result bits
  before reaping, and releases the worker's trapframe mapping.
- Policy: `wait` reaps process children only; `join` reaps thread contexts.
  `kill(pid)` resolves the target's `tgid` and stops every context in that
  process group. The leader waits for workers to stop and reaps them before it
  becomes waitable.
- Policy: `fork` creates a single-threaded child from the calling context.
  The child remains parented to that caller, so a worker can `wait` for a
  child it forked and can `exec` that child independently.
- Policy: `exec` is rejected while sibling contexts remain. `addrspace.group_lock`
  serializes clone publication against exec's refcount check and reservation;
  `execing` stays set while the image is built, so later clones fail until
  commit or failure cleanup. The same group gate closes clone admission during
  process-wide shutdown. This prevents a newly published thread from entering
  a page table while it is being replaced or torn down.
- The user-space `thread.c` change only bridges Member C's existing API to the
  exact-TID join and full-width thread-exit syscalls; it does not change the
  mutex or stack-allocation design.

### Verification evidence

Built the kernel and guest programs, then ran `lifecyclecheck` and
`usertests -q` under QEMU with `CPUS=4`. `lifecyclecheck` passed all seven
policy checks: wait/thread separation, exact-TID join and 64-bit retval, exec
rejection with a live sibling, worker fork/wait/exec, reservation cleanup after
a missing executable, 100 clone/join cycles around rejected exec, and
process-wide kill followed by parent wait/reap. It printed
`lifecyclecheck: SUMMARY PASS (7 policy checks)`. The regression suite ended
with `ALL TESTS PASSED`. `threadtest` and `leaktest` also passed their
multithreaded return/counter and repeated lifecycle checks on `CPUS=4`.

### Policy decisions and rationale

- **Join is TID-specific and group-scoped.** This prevents a caller from
  reaping another process's thread or consuming a different worker's result.
  `uint64` preserves pointer-sized results on RV64.
- **`wait` is for child processes; `join` is for threads.** This preserves the
  existing integer process exit status while giving thread results a separate
  full-width contract.
- **`kill(pid)` is process-wide.** Shared process state cannot safely continue
  under an arbitrary subset of contexts after a kill request.
- **`fork` uses the calling context.** The new child inherits that context's
  memory/register image and parent relation, and starts single-threaded; this
  lets a worker own and wait for its child.
- **`exec` requires siblings to be joined first.** Swapping the shared page
  table while siblings use it would invalidate their execution state. The group
  lock and reservation close the clone/exec check-then-publish race.

The stress test exercises the public clone/exec policy on four CPUs but does
not deterministically inject every internal instruction-level interleaving.
