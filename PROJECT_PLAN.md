# Project 26105 workload tracker

## Project schedule

| Week | Deliverable | Owner | Reviewer |
| --- | --- | --- | --- |
| 1 | Repository setup and build environment | All | All |
| 2 | Design proposal draft | All | All |
| 3 | Design review and 2–3 page proposal | All | All |
| 4 | Memory and lifecycle specification complete | A/B | B/A |
| 5 | Midterm checkpoint and demo | All | All |
| 6 | User-thread library prototype | C | A/B |
| 7 | Kernel thread implementation + tests | A/B | B/A |
| 8 | Code freeze and final tag | All | All |
| 9 | Final demo and report | All | All |

## Workload tracker

| Member | Task ID | Description | Estimated hours | Dependencies | Reviewer | Acceptance criteria | Status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A | A-01 | Environment and repo setup; upstream validation | 3 | None | B | Build environment documented and upstream repo confirmed | In progress |
| A | A-02 | Memory and page-table ownership design | 8 | A-01 | B | Design note complete, invariants stated | Not started |
| A | A-03 | Reference-counted resource lifetime checks | 10 | A-02 | B | Leak checks and cleanup paths described | Not started |
| B | B-01 | Kernel thread lifecycle design | 8 | A-01 | A | clone/join semantics documented | Not started |
| B | B-02 | Thread/process state and exit semantics | 10 | B-01 | A | `exit`, `kill`, `wait` interactions documented | Not started |
| B | B-03 | Thread reaping and join implementation | 12 | B-02 | A | Correct lifecycle and reaping rules | Not started |
| C | C-01 | User thread library API design | 6 | B-01 | A | User-visible API and syscall boundaries defined | Not started |
| C | C-02 | Mutex primitive and RISC-V atomics | 8 | C-01 | B | Mutex correctness rules and atomic use documented | Not started |
| C | C-03 | Stress harness and benchmark plan | 10 | B-03, C-02 | A | Functional and benchmark tests defined | Not started |

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

The table above is an initial planning estimate. Actual hours should be tracked weekly and the workload rebalance should be adjusted if one member is overloaded or if a design issue delays the plan.
