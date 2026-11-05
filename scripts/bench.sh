#!/usr/bin/env bash
# VERITY GRAPH — benchmark runner (real numbers into out/validation/).
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

BUILD="$ROOT_DIR/build"
BENCH="$BUILD/verity_bench.exe"
if [ ! -x "$BENCH" ]; then BENCH="$BUILD/verity_bench"; fi
if [ ! -x "$BENCH" ]; then
  echo "verity_bench not built — run scripts/build.sh first (target verity_bench)" >&2
  exit 1
fi
mkdir -p out/validation
"$BENCH" | tee "out/validation/benchmarks_$(date +%Y%m%d_%H%M%S).txt"
