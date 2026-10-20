# Architecture overview

This document records the architecture assumptions for the kernel-thread project in the xv6-riscv repository.

## Kernel/user-space boundary

The kernel owns privileged state, including page tables, process metadata, trap handling, and scheduling. User programs run in user mode and invoke kernel services via syscalls.

The project keeps the stock xv6 boundary intact. New functionality expands the semantics of process/thread lifecycle, but it does not move ownership of privileged state into user space.

## Thread and process responsibilities

The project distinguishes between:

- Process: a traditional xv6 process with a user page table and process metadata
- Kernel thread: a schedulable execution context in the same address space, sharing page table and resources with a parent process

This distinction is required because the upstream `fork` and `exit` semantics are process-centric, while the project introduces a new shared-address-space execution model.

## Page-table sharing

A key design question is how kernel threads share memory with the rest of the process. The project proposes that a kernel-thread group shares a single page table and a shared process metadata record while retaining separate thread-local execution state and join bookkeeping.

The design requires:

- reference counting for page tables
- reference counting for any shared memory resources
- careful lifetime tracking during thread exit and process exit

## Synchronization boundaries

Synchronization is required at two levels:

1. Kernel-level locking for shared process metadata and page-table lifetime bookkeeping
2. User-level synchronization primitives built on top of RISC-V atomic instructions

Because xv6 already uses spin locks and the RISC-V atomics instruction set, the thread library should target those mechanisms rather than reimplement locking incorrectly.

## Open design decisions

The following items are intentionally left open until design review:

- whether each thread should have a private kernel stack or a shared kernel stack segment under a process group
- how to represent a joinable thread lifecyle separate from a process
- whether `wait` semantics are preserved for process-level children only or extended for thread groups
- how TLS via `tp` can be implemented without breaking trap and context-switch logic

These items must be resolved with explicit acceptance criteria before kernel code changes are merged.
