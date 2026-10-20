# Development setup

This document records the project environment and the required setup for building and testing xv6-riscv with the kernel-thread project.

## Supported host environment

The project is intended for a Unix-like development environment, typically:

- Linux (Ubuntu, Debian, or similar)
- macOS with a compatible RISC-V toolchain and QEMU setup
- A machine with at least 8 GB RAM for repeated build/test cycles

## Required toolchain

Install the RISC-V GNU toolchain and ensure the compiler tools are on `PATH`.

Typical prefixes are:

- `riscv64-unknown-elf-`
- `riscv64-linux-gnu-`
- `riscv64-unknown-linux-gnu-`

Check availability with:

```bash
which riscv64-unknown-elf-gcc
which qemu-system-riscv64
```

The upstream xv6 Makefile auto-detects one of the supported tool prefixes.

## QEMU requirements

The project expects QEMU to support RISC-V virtual machines, specifically `qemu-system-riscv64`.

Minimum supported version:

- QEMU 7.2 or newer

Check:

```bash
qemu-system-riscv64 --version
```

## Build commands

From the repository root:

```bash
git status
make clean
make qemu CPUS=1
```

For multiprocessing tests:

```bash
make qemu CPUS=4
```

## Common troubleshooting

### Toolchain not found

If `make` reports that it cannot find a RISC-V compiler or binutils, install the toolchain and add it to your `PATH`.

### QEMU version too old

If `make` exits with a version check failure, install a newer QEMU release or point the environment to the correct binary.

### Build fails after project changes

- Check whether the modification touched upstream build files.
- Confirm the repository remains based on the upstream `riscv` branch.
- Run `git status` and review uncommitted changes before editing.

### Serial output not visible

The default xv6 qemu target is `-nographic`, which is appropriate for serial debugging.

## CI limitation note

A GitHub Actions workflow may install the toolchain and QEMU automatically, but it must be validated in the actual CI environment. If the runner does not provide QEMU or the RISC-V toolchain, the workflow should document that limitation rather than claim the tests pass.

## Initial project environment status

At the time of initialization, the repository is a clean upstream checkout aside from local modifications already present in the working tree. No project-specific kernel or user implementation should be applied until the design review is complete.
