# Week 2 report (draft)

## Goals

- finalize the design proposal
- document memory ownership and page-table lifetime invariants
- review the process and syscall interfaces that will be affected

## Status

This is a placeholder report and should not be filled with fabricated measurements or completed milestones.

## Planned work

- design review for `clone` and `join`
- lock-order and resource-lifetime analysis
- user-thread API draft

## Risks

- inconsistent semantics between process and thread exit
- stale page-table references after join or kill
- inadequate cross-review of kernel lifecycle code
