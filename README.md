# Project 26105 — xv6-riscv Kernel Threads

This repository is a project fork of the official MIT xv6-riscv tree and is intended for the course project "Kernel threads and a user thread library". The goal is to extend the stock xv6 kernel with kernel-thread primitives, reference-counted resource lifetime management, and a user-level thread library that uses appropriate RISC-V atomics and syscalls.

Important: this repository preserves the upstream xv6 layout and build flow. Project-specific files are added alongside the upstream code rather than replacing or renaming the stock kernel components.

## Scope

The project targets the following behaviors:

- `clone` and `join` for kernel threads that share a page table.
- Reference-counted page-table and resource lifetime management.
- Correct handling of `exit`, `kill`, and `wait` in a multithreaded process.
- A user-thread library that exposes thread creation, join, and synchronization primitives.
- Mutex synchronization using supported RISC-V atomic instructions.
- Validation via stress tests and parallel benchmarks.
- Stretch goal: thread-local storage via `tp`.

## Prerequisites

The build environment must provide:

- A RISC-V GNU toolchain (`riscv64-unknown-elf-*` or equivalent)
- QEMU `qemu-system-riscv64` version 7.2 or newer
- `make`, `gcc`, `perl`, `python3`, and standard Unix build tools

The upstream xv6 README states the requirement for the RISC-V GNU toolchain and QEMU. If these dependencies are not installed, the project cannot be built or tested reliably.

## Repository structure

- `kernel/` — upstream xv6 kernel sources and required modifications
- `user/` — upstream user programs and project-specific user-space tests
- `mkfs/` — filesystem toolchain used to build `fs.img`
- `docs/` — architecture notes, weekly reports, benchmarks, and report scaffolding
- `scripts/` — host-side orchestration scripts for qemu and test collection
- `tests/` — host and guest test plans, organized by area
- `.github/` — pull request and CI workflow scaffolding

## Build and run

From the repository root:

```bash
make clean
make qemu CPUS=1
```

For a multiprocessor run:

```bash
make qemu CPUS=4
```

The default upstream xv6 targets remain authoritative. Project-specific targets should be added only when they are compatible with the official workflow.

## Test instructions

Host-side scripts are kept separate from xv6 guest programs. The general flow is:

```bash
./scripts/run-tests.sh
```

If a single host-side script launches QEMU, it must handle serial output and report PASS/FAIL explicitly; otherwise, it should be documented as not reliable in the current host environment.

## Team responsibilities

- Member A — memory management and reference counting
- Member B — kernel-thread lifecycle and syscall semantics
- Member C — user-thread library, synchronization, and integration tests

See `docs/design/ownership.md` for the initial ownership model and `PROJECT_PLAN.md` for the workload tracker.

## Milestones

- Week 1: repository and development environment setup
- Week 3: design proposal, 2–3 pages
- Week 5: midterm checkpoint and 10-minute demo
- Week 8: code freeze and final git tag
- Week 9: final demo, 15–20 page report, and individual contribution statements

## Current implementation status

This repository is currently in the initial project scaffolding phase. The upstream xv6 repository remains intact and buildable as provided, and the project-specific documentation, ownership model, and test planning are being established before kernel implementation begins.

## Contribution policy

- `main` must remain buildable.
- Work occurs on feature branches.
- Every change must be reviewed by a different team member.
- Generated binaries, object files, logs, and editor settings are not committed.

## Reference upstream version

The upstream version in this repository is tracked as the official `mit-pdos/xv6-riscv` branch `riscv` at commit `06aad25`.

This repository does not currently declare a project-specific upstream tag beyond this checked-out official revision. The exact instructor-required tag is not yet known and is left as a placeholder in project planning documents.
