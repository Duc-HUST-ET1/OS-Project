#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

mkdir -p "$ROOT_DIR/results"

echo "[xv6-thread-project] collecting results to $ROOT_DIR/results"
find "$ROOT_DIR" -maxdepth 3 \( -name '*.log' -o -name '*.out' -o -name '*.txt' \) -print | while read -r f; do
  cp "$f" "$ROOT_DIR/results/" 2>/dev/null || true
done

echo "PASS: results collection initialized"
