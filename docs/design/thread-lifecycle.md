# Thread lifecycle design

This document records the implemented kernel-thread lifecycle and shared-address-space policy in this xv6-riscv project.

## State model

Thread contexts use the existing proc states:

- `UNUSED`
- `USED`
- `SLEEPING`
- `RUNNABLE`
- `RUNNING`
- `ZOMBIE`

The scheduler ID (`pid`) is the TID; `tgid` identifies the process group. `clone` allocates a private proc and trapframe, maps that trapframe into the shared page table, initializes the group metadata, and publishes the context as `RUNNABLE` only after setup completes.

## Metadata and lock order (B1)

Each schedulable context has an explicit identity independent of its shared
address space: a thread-group ID, a thread marker, and a 64-bit exit result.
The existing `pid` remains the scheduler-visible ID. A thread does not own or
free the group page table merely because its own context exits.

The lifecycle lock order is:

1. `addrspace.group_lock` serializes clone admission against exec reservation
   and process-wide shutdown. It is acquired before `addrspace.lock`.
2. `wait_lock` precedes proc locks for parent/child and join relationships.
3. Acquire one proc lock at a time; never acquire another proc lock while
   holding one.
4. `addrspace.lock` protects shared page-table and reference-count changes.
   Clone releases it before taking `wait_lock` to publish the new context.

Clone allocates its private proc resources before entering the group gate, then
keeps the gate until the context is fully initialized and `RUNNABLE`. Exec
checks the reference count and reserves `execing` under the gate, then builds
the replacement image without holding the gate. The reservation makes further
clones fail. At commit, exec reacquires the gate and replaces the image. Group
shutdown sets `exiting` under the same gate before marking contexts killed.
This prevents clone publication from crossing either lifecycle boundary.

## Execution

Kernel threads execute in the same address space as the original process, with thread-local bookkeeping for:

- scheduler state
- stack pointer / context frame
- exit status
- wait/join state

## Termination

Thread exit does not automatically destroy the shared page table. The implementation distinguishes between:

- thread exit, which ends one schedulable context
- process exit, which tears down the entire shared address space

This distinction is central to the project.

## Join and resource reclamation

`join` waits for a specific thread to terminate and reclaims thread-local resources without invalidating the shared page table while other threads still exist. The joiner clears the thread-slot metadata after the thread is reaped.

## Implemented lifecycle details

- Each context has its own kernel stack and private trapframe page. The shared
  user page table maps each trapframe at that context's `trapframe_va`.
- TLS is passed in `tp` by the clone syscall and the initial argument is passed
  in `a0`.
- Thread contexts are marked explicitly in `struct proc`. `wait` skips them;
  `join` reaps them by exact TID.
- `thread_exit` records a 64-bit result and leaves the shared page table alive.
  The joining context unmaps the worker trapframe and releases its proc slot.
- Process-wide exit/kill closes clone admission and stops the whole group. The
  leader reaps worker contexts before becoming a zombie for its parent.
- `fork` creates a single-threaded process child from the calling context.
  `exec` is rejected until sibling contexts are joined.

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
  aligned initial stack pointer addressing the startup record, and the third
  argument initializes both `tp` and child `a0` for the user trampoline.
- The child stays `USED` while allocation, trapframe mapping, register setup,
  file/cwd references, and parent linkage complete. The final transition to
  `RUNNABLE` occurs under the child proc lock after acquiring `wait_lock`.
- A failed allocation or trapframe mapping frees the unpublished child and
  leaves the parent's page table and scheduler state untouched.

## R3/R4 policy decisions and implementation

- `thread_exit(uint64)` ends one thread and records all 64 result bits.
  `join(tid, uint64 *)` accepts only a live thread ID in the caller's group,
  copies the result before freeing the proc and trapframe slot, and returns the
  exact TID.
- `wait` reaps process children only. Thread contexts are reaped by `join`.
- `kill(pid)` resolves the target's `tgid`, closes clone admission, and marks
  every context in that process group killed. The parent later reaps the group
  leader through `wait`.
- Process `exit` terminates the entire group. The leader waits for worker
  contexts to become zombies and reclaims them before becoming waitable.
  Worker code that wants to end only itself must call `thread_exit`.
- `fork` copies the calling context's address space and register state into a
  new single-threaded process. The child remains a process child of the caller,
  so a worker can wait for a child it created.
- `exec` is rejected while any sibling context still owns the shared address
  space. The caller must join siblings first. The group gate plus `execing`
  reservation prevents a concurrent clone from slipping into the image swap.

These choices keep `wait`'s integer process status separate from `join`'s
RV64-width pointer result, avoid freeing a page table still used by siblings,
and make clone admission atomic with exec and group shutdown. The guest
`lifecyclecheck` covers each listed policy, including 100 clone/join cycles
against a rejected exec attempt. This is stress coverage of the public policy,
not a deterministic scheduler hook at every instruction-level race boundary.
