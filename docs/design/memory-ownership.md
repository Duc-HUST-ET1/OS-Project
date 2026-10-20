# Memory ownership and reference counting

This document captures the initial rules for page-table and resource ownership in the multithreaded xv6 project.

## Page-table ownership

The process page table is shared among all threads created within a process group. The page table must not be freed as soon as a single thread exits; instead, it remains valid until the last owning thread or process has released it.

## Reference-count invariants

The implementation should maintain the following invariants:

1. Every shared page table and associated memory resource has a reference count.
2. Creation of a new thread that shares the page table increments the count.
3. Thread exit decrements the count only for thread-local bookkeeping, not for the process-shared page table.
4. Process exit decrements the final count and reclaims the resources only when the last reference is released.
5. Join operations must not release a resource before all readers/writers have completed.

## Locking rules

Because page-table and process metadata are shared across threads, all ownership updates must occur under the project's synchronization discipline.

Rules:

- never update a reference count without holding the relevant lock
- avoid lock-order inversions between process locks and page-table allocation locks
- ensure `kill` and `wait` do not race with thread reaping or page-table release

## Allocation failure and cleanup

Resource allocation failure must not leave partially initialized thread state behind. The cleanup path should:

- release any successfully allocated per-thread metadata
- decrement any refcounts incremented earlier in the creation path
- leave the process in a consistent state if a thread cannot be created

## Open issues

- exact mapping between thread count and page-table reference count
- difference between process-owned and thread-owned memory pages
- whether free-page accounting is measured during kernel-thread lifecycle stress tests

These decisions are intentionally flagged for explicit review before implementation begins.
