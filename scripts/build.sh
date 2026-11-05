#!/usr/bin/env bash
# VERITY GRAPH build script (Linux / macOS / WSL / Git Bash on Windows).
# Resolves all paths from the script location; safe with spaces in paths.
# Usage: scripts/build.sh [--sanitize] [--werror] [--tests] [--clean]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"

SANITIZE=OFF; WERROR=OFF; RUN_TESTS=0; CLEAN=0
for arg in "$@"; do
  case "$arg" in
    --sanitize) SANITIZE=ON ;;
    --werror)   WERROR=ON ;;
    --tests)    RUN_TESTS=1 ;;
    --clean)    CLEAN=1 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done

# --- locate a C++ compiler --------------------------------------------------
find_cxx() {
  if [ -n "${CXX:-}" ] && command -v "$CXX" >/dev/null 2>&1; then echo "$CXX"; return 0; fi
  if command -v clang++ >/dev/null 2>&1; then command -v clang++; return 0; fi
  if command -v g++ >/dev/null 2>&1; then command -v g++; return 0; fi
  for p in "/c/Program Files/LLVM/bin/clang++.exe" "/c/Program Files/LLVM/bin/clang++" \
           "/usr/bin/g++" "/usr/local/bin/g++" "/mingw64/bin/g++"; do
    if [ -x "$p" ]; then echo "$p"; return 0; fi
  done
  echo ""; return 1
}

CXX_BIN="$(find_cxx)" || { echo "ERROR: no C++ compiler found (clang++ or g++ required)." >&2; exit 1; }
echo "==> Compiler: $CXX_BIN"

# Windows+clang needs an RC compiler; prefer llvm-rc next to clang++.
RC_ARGS=()
if [[ "$CXX_BIN" == *"clang++"* ]]; then
  RC_BIN=""
  if command -v llvm-rc >/dev/null 2>&1; then RC_BIN="$(command -v llvm-rc)"
  elif [ -x "$CXX_BIN" ]; then
    RC_CANDIDATE="$(dirname "$CXX_BIN")/llvm-rc.exe"
    [ -x "$RC_CANDIDATE" ] && RC_BIN="$RC_CANDIDATE"
    RC_CANDIDATE="$(dirname "$CXX_BIN")/llvm-rc"
    [ -x "$RC_CANDIDATE" ] && RC_BIN="$RC_CANDIDATE"
  fi
  if [ -n "$RC_BIN" ]; then RC_ARGS=(-DCMAKE_RC_COMPILER="$RC_BIN"); fi
fi

if [ "$CLEAN" = "1" ]; then rm -rf "$BUILD_DIR"; fi
mkdir -p "$BUILD_DIR"

echo "==> Configuring (CMake + Ninja)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
  -DCMAKE_CXX_COMPILER="$CXX_BIN" \
  "${RC_ARGS[@]}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVERITY_SANITIZE="$SANITIZE" \
  -DVERITY_WERROR="$WERROR"

echo "==> Building"
cmake --build "$BUILD_DIR"

if [ "$RUN_TESTS" = "1" ]; then
  echo "==> Running tests"
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi

echo "==> Done. CLI: $BUILD_DIR/verity(.exe)   Tests: $BUILD_DIR/verity_tests(.exe)"
