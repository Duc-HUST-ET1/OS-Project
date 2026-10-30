# Proposed syscall API

This document is intentionally a proposal and must be clearly distinguished from implemented upstream xv6 syscalls.

## Proposed interfaces

The following syscalls are planned for the project and are not part of the stock xv6 ABI unless explicitly added during implementation.

```c
int clone(void (*entry)(void), void *stack, void *tls);
int join(int thread_id, void **retval);
int thread_exit(uint64_t retval);
```

`clone` receives the user entry point explicitly, `stack` is the 16-byte-aligned
initial stack pointer that addresses the startup record, and `tls` initializes
the child's `tp` register. The kernel core is exposed as `kthread_create(entry,
stack, tls)`. B5 registers `clone` with syscall number 25 and adds the user
prototype and assembly stub. `join` and `thread_exit` require the lifecycle
operations from B4 before their handlers can be registered.

## R2 ABI decisions from Member C feedback

- A newly created child returns from the clone syscall with `a0 = 0`.
- The user library supplies a stack whose top is 16-byte aligned. The proposed
  startup record contains the start routine and its argument; a user
  trampoline invokes the routine and passes its pointer result to thread exit.
- Exit results and user addresses use RV64 `uint64` values end to end. `join`
  copies exactly eight bytes to the caller's result pointer.
- `join` returns the reaped TID on success and `-1` on failure.

The explicit entry argument resolves the discrepancy between the fixed
user-trampoline contract and the earlier `clone(flags, stack, tls)` summary.
The user library passes `thread_trampoline` as `entry`; the kernel does not
interpret function pointers or startup records.

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
