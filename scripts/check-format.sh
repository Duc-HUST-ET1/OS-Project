#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "FAIL: clang-format is not installed" >&2
  exit 2
fi

clang-format --dry-run --Werror kernel/*.[ch] user/*.[ch] mkfs/*.c
