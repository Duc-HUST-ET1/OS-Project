# Proposed syscall API

This document is intentionally a proposal and must be clearly distinguished from implemented upstream xv6 syscalls.

## Proposed interfaces

The following syscalls are planned for the project and are not part of the stock xv6 ABI unless explicitly added during implementation.

```c
int clone(uint64_t flags, void *stack, void *tls);
int join(int thread_id, void **retval);
int thread_exit(int status);
```

The exact syscall numbers, argument ordering, and error semantics will be determined by the design review and the actual xv6 syscall table.

## Arguments and return values

- `clone`: creates a new kernel thread that shares the current process address space.
- `join`: blocks until the target thread exits and returns its exit status.
- `thread_exit`: terminates a single thread while leaving shared process resources alive if other threads remain.

Return values:

- non-negative value on success
- `-1` on invalid arguments or failure with an error code in the caller-visible convention used by the project

## Distinction from implemented interfaces

The upstream xv6 kernel does not currently define these syscalls as part of the stock ABI. This document reflects a design target, not a claim that the repository already implements the interface.

## Required review before implementation

- mapping to `syscall` numbers in `kernel/syscall.c`
- user-space wrapper definitions in `user/usys.pl` and `user/user.h`
- whether the thread API should use traditional process-surrogate semantics or a dedicated thread ID type
- whether `kill` and `wait` require changes for multithreaded process semantics

## Error cases

The implementation should clearly handle:

- invalid thread IDs
- join on a thread that was never created or already reaped
- attempts to join a process rather than a thread
- stack or argument validation failures
- resource-allocation errors during thread creation
