# Week 6 report (draft)

## Goals

- implement the user-space thread library prototype
- define synchronization primitives and testing hooks
- integrate user-level API with the kernel design

## Status

This is a placeholder report. No finalized implementation is recorded here.

## Planned work

- review RISC-V atomic requirements for mutexes
- draft the user-level API and syscall wrappers
- align user testing with kernel lifecycle semantics

## Risks

- incorrect lock semantics or atomic ordering
- mismatched syscall contract between user space and kernel
- insufficient testing coverage for low-level races
