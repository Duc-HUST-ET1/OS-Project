# Week 3 report (draft)

## Goals

- finalize the design proposal and architecture review
- resolve open page-table ownership questions
- confirm the syscall and user-library boundary

## Status

This report is retained as a placeholder for Week 3. Member B's R3/R4
implementation results and test evidence are recorded in `week-04.md`.

## Planned work

- confirm thread vs. process semantics
- document API and lifecycle invariants
- prepare midterm checkpoint materials

## Risks

- design mismatch between memory model and scheduler state
- incomplete join semantics when a thread exits
- inconsistent ownership if `kill` or `wait` races with thread reaping
