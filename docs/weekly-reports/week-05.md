# Week 5 report — Member B

**Owner:** Trần Anh Đức — Member B — 20233337
**Scope:** Task 2 only. This entry records Member B's kernel lifecycle work; it does not claim work owned by Members A or C.

## Task

Present the completed kernel thread lifecycle policy (`kill`, `wait`, `fork`,
`exec`, exact-TID `join`, and 64-bit thread return values); run
`lifecyclecheck` and `usertests -q` on the four-CPU demo image; review lock
ordering and reap paths.

## Work completed

- Prepared the lifecycle explanation: `wait` reaps process children, while
  `join(tid, retval)` reaps a thread in the caller's group by its exact TID and
  copies all 64 bits of the return value. `kill(pid)` and process `exit` stop
  the whole thread group. `fork` creates a single-threaded child from the
  calling context. `exec` fails while sibling threads remain, and the caller
  must join them first.
- Reviewed the clone/exec/shutdown gate. `group_lock` serializes clone
  publication against the `execing` reservation and group shutdown. Clone
  releases the address-space lock before it takes `wait_lock` to publish the
  thread. Join and process reap use `wait_lock` before proc locks; `freeproc`
  then releases the address-space reference while holding the proc lock.
- During review, found a stale-address-space window in `kkill`: it saved the
  `addrspace` pointer under a proc lock, then used the pointer after releasing
  that lock. A concurrent reap and descriptor reuse could make that request
  apply to a later process. Updated `kkill` to take the group gate, revalidate
  the exact PID/TGID/address-space association under the proc lock, and set the
  shutdown flags before releasing the gate. It no longer uses the descriptor
  after releasing the gate.
- Added `user/killrace.c`, a Member B stress test that runs a killer thread
  while the parent repeatedly exits/reaps a target and forks a probe child.
  Each probe creates and joins a thread in its newly allocated address space.
- Ran `killrace`, `lifecyclecheck`, and the kernel regression suite on the
  rebuilt demo image with four QEMU harts. All passed after the `kkill` change.

## Week 5 change compared with Week 4

Week 4 already recorded the same seven `lifecyclecheck` policy checks and a
passing `usertests -q` run. The Week 5 contribution is the source change in
`kernel/proc.c`: `kkill` now revalidates the target PID, TGID, and address-space
association under the group gate before setting group shutdown state. The
Week 5 reruns show that this change preserves the existing policy behavior and
kernel regression result. The new `killrace` adds targeted concurrent
kill/reap/address-space-reuse stress, but it is not a deterministic scheduler
hook and cannot guarantee every run hits the narrow lookup-to-gate race
window. The code diff is the direct evidence of the implementation change.

## Build and reproduction

From the repository root, build the kernel and user image:

```sh
make kernel/kernel fs.img
```

Start the final local demo image with four harts:

```sh
make qemu CPUS=4
```

At the xv6 `$` prompt, run these commands sequentially and wait for each to
return to the prompt:

```text
killrace
lifecyclecheck
usertests -q
```

## Results

`lifecyclecheck` completed all seven policy checks:

```text
lifecyclecheck: PASS wait does not reap thread contexts
lifecyclecheck: PASS exact-TID join returned 0x123456789ABCDEF0
lifecyclecheck: PASS exec rejected while sibling thread exists
lifecyclecheck: PASS worker fork child exec
lifecyclecheck: PASS worker forked, waited for, and exec'd child
lifecyclecheck: PASS failed exec released clone reservation
lifecyclecheck: PASS clone/exec exclusion; 100 clone/join cycles
lifecyclecheck: PASS kill(pid) stopped group and wait reaped leader
lifecyclecheck: SUMMARY PASS (7 policy checks)
```

The new targeted stress test completed:

```text
killrace: PASS 500 concurrent kill/reap and addrspace-reuse rounds
killrace: PASS clone/join in every reused child address space
killrace: SUMMARY PASS (2 stress checks)
```

The regression command finished with:

```text
ALL TESTS PASSED
```

## Midterm presentation outline

1. Explain why the kernel distinguishes a process ID/TGID from a schedulable
   thread ID/TID, and why thread exit cannot free a page table shared by
   siblings.
2. State the policy pairings: `wait`/process child versus `join`/exact thread
   TID; `kill`/group shutdown; worker `fork`/single-threaded child from the
   calling context; `exec`/reject while siblings remain; `thread_exit`/64-bit
   return value.
3. Explain the race boundary: `group_lock` protects clone admission against
   exec reservation and group shutdown. The `kkill` revalidation now follows
   `group_lock -> proc lock -> addrspace lock`, and does not retain the
   descriptor after leaving the group gate.
4. Run `killrace` to show the new concurrent kill/reap stress, then
   `lifecyclecheck` for the seven policy checks, and `usertests -q` for the
   integrated kernel regression result.

## Review status and limitations

The lock/reap review above is Member B's source-level review. A peer
cross-review with Member A still needs their confirmation and is not claimed
as complete here. `lifecyclecheck` exercises the public clone/exec policy for
100 clone/join cycles. `killrace` increases concurrency around kill, reap, and
address-space reuse, but neither test deterministically forces every
instruction-level interleaving. A pass is evidence of stress coverage, not a
proof that the race is impossible.
