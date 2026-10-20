#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

CPUS="${CPUS:-1}"

echo "[xv6-thread-project] Launching xv6 on qemu with CPUS=${CPUS}"

if ! command -v qemu-system-riscv64 >/dev/null 2>&1; then
  echo "FAIL: qemu-system-riscv64 is not installed or not on PATH" >&2
  exit 2
fi

make qemu CPUS="${CPUS}"
