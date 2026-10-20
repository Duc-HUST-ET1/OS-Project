# Benchmark plan

This file documents the required benchmark strategy for the project.

## Parallel sum with eight threads

The project must validate the expected performance improvement over a single-thread baseline. The test should:

- use eight worker threads
- combine results with a reduction operation
- compare against the one-thread result and report relative speedup

## Measurement policy

Each workload should be executed at least five times to record a mean and spread. The measured data must be intentionally reported rather than inferred.

## Required benchmark dimensions

- 1-thread baseline
- 8-thread parallel run
- CPUS=1 and CPUS=4 validation
- repeated stress runs to assess stability

## Pass criteria

A benchmark passes only if the output is verified and the reported timing data is collected from an actual execution.

## Important note

Benchmarking itself is not a substitute for correctness testing. A benchmark result is only useful when paired with verified functional tests.
