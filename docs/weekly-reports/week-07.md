# Week 7 report (draft)

## Goals

- implement kernel-thread lifecycle primitives as agreed by the team
- validate shared page-table and resource ownership behavior
- begin cross-review and regression checks

## Status

This is an explicit placeholder; no implementation result is recorded here.

## Planned work

- review `clone`, `join`, and exit semantics
- validate resource cleanup and reclaim logic
- run targeted and regression tests

## Risks

- scheduler and trap interaction bugs
- use-after-free in thread cleanup paths
- unexpected regressions in the stock xv6 process model
