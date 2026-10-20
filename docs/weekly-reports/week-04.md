# Week 4 report (draft)

## Goals

- finalize the memory model and page-table ownership rules
- confirm the thread metadata and resource-lifetime invariants
- review the initial syscall and join design

## Status

This is a placeholder report. No implementation claims are made.

## Planned work

- review reference counting with thread exit and process exit
- confirm cleanup paths on allocation failure
- align cross-review feedback into the design proposal

## Risks

- double-free during resource teardown
- lock-order inversion between process and memory locks
- stale resource ownership after a failed thread creation path
