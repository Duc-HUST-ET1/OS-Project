# Contributing guide

## Branch and review policy

- `main` must remain buildable at all times.
- Use feature branches such as:
  - `feature/refcount-pagetable`
  - `feature/kernel-clone-join`
  - `feature/user-thread-mutex`
  - `test/thread-lifecycle`
  - `test/thread-stress-benchmark`
- Require review by a different team member before merging.
- Use task IDs in branch names or commit messages when practical.

## Commit hygiene

- Do not commit generated binaries, object files, temporary logs, or personal editor settings.
- Do not commit secrets, local environment files, or machine-specific configuration.
- Avoid unrelated formatting changes in upstream files.
- Never force-push shared branches without team agreement.

## Merge expectations

- The code should preserve the stock xv6 structure.
- Review should confirm the design invariants and resource-lifetime rules.
- Tests must be executed and recorded before final merge.

## Working with the upstream project

This repository is a fork of the official MIT xv6-riscv tree. Changes should remain compatible with upstream behavior except where the project explicitly extends it.

Never silently change `fork`, `exit`, `wait`, or `kill` semantics without an explicit design review.
