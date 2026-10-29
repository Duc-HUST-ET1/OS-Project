# Thread lifecycle design

This document describes the proposed lifecycle for kernel threads and a shared-address-space execution model in xv6-riscv.

## Proposed state machine

A thread is expected to transition through states such as:

- `UNALLOCATED`
- `RUNNABLE`
- `RUNNING`
- `SLEEPING`
- `ZOMBIE`
- `DEAD`

The exact state encoding should map cleanly onto the existing xv6 process state model while preserving `fork` semantics for ordinary processes.

## Creation

Thread creation will likely require a dedicated syscall such as `clone` that:

- allocates a thread descriptor
- duplicates or shares the page table according to the design
- initializes thread-local execution metadata
- records waiting/join state

The exact creation semantics must be reviewed against the interaction with `fork` and the scheduler.

## Metadata and lock order (B1)

Each schedulable context needs an explicit identity independent of its shared
address space. The implementation will track a thread-group ID, whether a
proc is a thread or a process leader, and a 64-bit exit result. The existing
`pid` remains the scheduler-visible ID. A thread must not own or free the
group page table merely because its own context exits.

The lock order for lifecycle operations is:

1. `wait_lock` before any proc lock when examining parent/child or join
   relationships, following the existing `kwait` discipline.
2. A single proc lock at a time while changing that proc's state or killed
   flag. Do not hold one proc lock while acquiring another proc lock.
3. `thread_vm_lock` may follow `wait_lock` and one proc lock for trapframe
   unmapping during reaping. Creation and `sbrk` take `thread_vm_lock` alone
   and release it before touching parent/child proc locks.

The first implementation must preserve this order in allocation failure,
publication to the scheduler, and future reaping paths. Existing `allocproc`
per-proc page allocation remains under its selected proc lock, as in the base
xv6 allocator path.

## Execution

Kernel threads will execute in the same address space as the original process, but they need thread-local bookkeeping for:

- scheduler state
- stack pointer / context frame
- exit status
- wait/join state

## Termination

Thread exit must not automatically destroy the shared page table. The design must distinguish between:

- thread exit, which ends one schedulable context
- process exit, which tears down the entire shared address space

This distinction is central to the project.

## Join and resource reclamation

`join` must wait for a specific thread to terminate and reclaim thread-local resources without invalidating the shared page table while other threads still exist. The joiner should also be responsible for clearing the thread-slot metadata after the thread is reaped.

## Unresolved design decisions

The design is intentionally incomplete in these areas and must be resolved before implementation:

- whether a cloned thread receives a separate kernel stack or uses a shared kernel stack layout
- how exactly a thread can be distinguished from a normal process for `kill` and `wait`
- the precise ordering between lock acquisition, wakeup, and resource freeing when a thread exits
- whether the implementation will introduce a new thread type into the existing `struct proc`
- whether TLS via `tp` is implemented as a stretch goal only

## Review criteria

The final implementation must guarantee:

- no double-free of page table resources
- no use-after-free when a thread joins another thread
- no silent changes to existing process semantics
- explicit handling of invalid thread IDs and join-of-non-child threads

## B1–B3 implementation decisions

- `pid` is the per-context scheduler ID; `tgid` identifies the leader's group.
- Each context has a private trapframe VA and physical page. The trampoline
  finds its trapframe through `sscratch`, set by `prepare_return()`. This is
  required because a shared page table cannot safely map all threads' trapframes
  at one fixed VA.
- `kthread_create(entry, stack, tls)` takes the user trampoline as `entry`, an
  aligned initial stack pointer addressing the startup record, and TLS in
  `tp`. Child `a0` starts at zero.
- The child stays `USED` while allocation, trapframe mapping, register setup,
  file/cwd references, and parent linkage complete. The final transition to
  `RUNNABLE` occurs under the child proc lock after acquiring `wait_lock`.
- A failed allocation or trapframe mapping frees the unpublished child and
  leaves the parent's page table and scheduler state untouched.

The `clone` syscall wrapper has been registered in B5. Group-wide exit, join/reaping, and
shared-address-space lifetime rules remain B4/B6 work and are required before
this kernel primitive is exposed as a complete user-facing thread API.

R2 review evidence should include the syscall/register contract, metadata and
lock-order invariants, allocation rollback behavior, unique trapframe handling,
and the final scheduler publication point.
