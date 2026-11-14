# Thread syscall API

The following thread syscalls extend the xv6 project ABI.

```c
int clone(void (*entry)(void *), void *arg, void *stack);
int join(int tid, uint64 *retval);
void thread_exit_sys(uint64 retval);
```

The user library exposes `thread_exit(void *)`; its assembly wrapper is named
`thread_exit_sys` to avoid colliding with that C function. Syscall numbers are
`clone=25`, `join=26`, and `thread_exit=27`.

`clone` creates a context sharing the current address space. Its stack pointer
is 16-byte aligned and points to the startup record read by the user trampoline.
The `arg` value initializes both child `a0` and `tp` (the library passes its
startup-record pointer there). The new context has a distinct TID and trapframe
mapping. It is made `RUNNABLE` only after proc metadata and shared mappings are
ready.

`thread_exit_sys` ends one context and records a full-width 64-bit result.
`join(tid, retval)` blocks for that exact TID in the caller's group, copies
eight bytes to `retval` when non-null, releases the thread's trapframe mapping
and proc slot, and returns the reaped TID. Invalid, foreign, non-thread, or
already-reaped TIDs return `-1`.

## Process policy for multithreaded groups

- `wait` reaps child processes and skips thread contexts.
- `kill(pid)` targets the process group containing that PID and requests exit
  for every context in that group. The parent's `wait` reaps the leader.
- `fork` creates a single-threaded child from the calling context. The child is
  parented to the caller, including when a worker performs the fork.
- `exec` returns `-1` while another context still references the address space.
  Join siblings first. A group lock and `execing` reservation serialize this
  check with clone publication.
- Process `exit` shuts down all contexts in the group; worker-only termination
  uses the dedicated thread exit syscall.

The group lock protects clone admission, exec reservation/commit, and group
shutdown. The address-space lock protects page-table, size, trapframe-slot, and
reference-count updates. The `wait_lock` is acquired before proc locks for
parent/child and join operations.
