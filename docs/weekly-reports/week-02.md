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
printed the parent TID, confirmed the child ran, and reported
`clonecheck: PASS (thread scheduled and reaped)`. This verifies the registered
clone path through scheduler publication and `wait` reclamation, but does not
claim join/thread-exit or full group-lifecycle coverage.

## Risks

- group-wide exit and final page-table release are not implemented in B1–B3
- concurrent file-table changes and `exec` need group-level integration
- inadequate cross-review of kernel lifecycle code
