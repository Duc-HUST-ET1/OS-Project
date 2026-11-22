# Project 26105 workload tracker

## Progress snapshot

**Baseline checked:** integrated `riscv` branch at merge commit `eb4d94a` (PR #13), 15 Nov 2026. The implementation work originally scheduled for Weeks 6–7 is already integrated, putting the team about 1–2 weeks ahead on implementation. The Week 5 midterm checkpoint remains the next course milestone; Weeks 5–7 should now focus on demonstration, integrated verification, review, and stabilization.

| Member | Integrated progress on `riscv` | Evidence present | Assessment |
| --- | --- | --- | --- |
| A — Đỗ Trọng Nam (20233541) | Shared `addrspace`, per-context trapframe slots, reference counting, shared-page-table growth, worker-thread `fork`/`exec` paths, and cleanup fixes. | `kernel/addrspace.c`, `kernel/addrspace.h`, `kernel/vm.c`, `user/leaktest.c`; leak test covers 1,000 sequential clone/join cycles, 400 threads in batches, heap grow/shrink, and 20 worker fork/exec rounds. | Core implementation is integrated and the leak test passed on CPUS=4. Re-run on the final demo image and review the refcount/cleanup paths. |
| B — Trần Anh Đức (20233337) | Kernel thread create/exit/join; exact-TID join with 64-bit retval; process-group kill/exit; wait/thread separation; worker-context fork policy; exec rejection with live siblings; group lock for clone/exec/exit admission. | `kernel/proc.c`, `kernel/exec.c`, lifecycle gate in `kernel/addrspace.*`, syscall files, `user/clonecheck.c`, `user/lifecyclecheck.c`; lifecyclecheck 7/7 and `usertests -q` passed on CPUS=4. | R3/R4 implementation and primary policy checks complete. Prepare the Week 5 demo and cross-review lock/reap paths. |
| C — Đỗ Phương Điệp (20233315) | User thread library and trampoline, atomic mutex, thread functional test, and parallel reduction benchmark for 1/2/4/8 workers. | `user/thread.c`, `user/thread.h`, `user/threadtest.c`, `user/parallel_sum.c`; benchmark matrix reports CPUS=1/4 results, and threadtest covers return value 42 plus the 4 × 10,000 mutex counter. | Prototype and benchmark are integrated. Tick resolution is coarse, so strengthen or repeat timing measurements before treating speedup values as final. |

### Test ownership note

`threadtest.c` currently contains Test 1 and Test 2. The 1,000-cycle clone/join stress test is in `user/leaktest.c` and belongs to A's address-space/resource-lifetime evidence. Do not attribute that test to `threadtest` or to Member C unless the test source is changed.

## Rebased project schedule

| Week | Revised target | Status / timing |
| --- | --- | --- |
| 1 | Repository and build environment | Complete |
| 2 | Design proposal and ownership/API decisions | Complete |
| 3 | Architecture review and lifecycle/address-space design | Complete |
| 4 | Integrated kernel lifecycle, shared address space, user library, and first tests | Complete; implementation stage reached about 1–2 weeks ahead of the original Week 6–7 coding schedule |
| 5 | Midterm checkpoint; demo integrated thread lifecycle and benchmark; capture final CPUS=4 evidence; review coverage matrix | Next milestone |
| 6 | Stabilization sprint: resolve review findings, rerun A/B/C-owned tests on merged `riscv`, improve benchmark timing confidence | Pull forward integration hardening |
| 7 | Target code-freeze candidate; full regression, documentation, and final acceptance review | Target one week ahead of original Week 8 freeze |
| 8 | Buffer for freeze findings, fixes, and final report | Schedule buffer |
| 9 | Official final demo and report | Keep course deadline |

## Workload tracker

| Member | Task ID | Description | Estimated hours | Dependencies | Reviewer | Acceptance criteria | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A | A-01 | Environment and repo setup; upstream validation | 3 | None | B | Build environment documented and upstream repo confirmed | In progress |
| A | A-02 | Memory and page-table ownership design | 8 | A-01 | B | Design note complete, invariants stated | Not started |
| A | A-03 | Reference-counted resource lifetime checks | 10 | A-02 | B | Leak checks and cleanup paths described | Not started |
| B | B-01 | Kernel thread lifecycle design | 8 | A-01 | A | clone/join semantics documented | Not started |
| B | B-02 | Thread/process state and exit semantics | 10 | B-01 | A | `exit`, `kill`, `wait` interactions documented | Not started |
| B | B-03 | Thread reaping and join implementation | 12 | B-02 | A | Correct lifecycle and reaping rules | Not started |
| C | C-01 | User thread library API design | 6 | B-01 | A | User-visible API and syscall boundaries defined | Completed |
| C | C-02 | Mutex primitive and RISC-V atomics | 8 | C-01 | B | Mutex correctness rules and atomic use documented | Completed |
| C | C-03 | Stress harness and benchmark plan | 10 | B-03, C-02 | A | Functional and benchmark tests defined | Completed |

## Risk log

- Unclear relationship between thread groups and process lifetime
- Risk of page-table double free during exit or join
- Lock-order inversions between process metadata and memory bookkeeping
- User-level mutex misuse due to incorrect atomic use
- CI environment may not provide the required toolchain and QEMU

## Mitigation

- Document invariants before implementation
- Require cross-review for every significant code change
- Keep test and benchmark plans explicit and reproducible
- Use the upstream build as the baseline and avoid silent semantic changes

## Notes

The original schedule was an initial estimate. Current status is based on code and test artifacts integrated into `riscv` as of merge PR #13, not on planned tasks alone. The Week 5 checkpoint date remains fixed; the team is using the early implementation progress to move stabilization and freeze readiness forward.
