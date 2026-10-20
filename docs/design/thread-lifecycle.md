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
