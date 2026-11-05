#!/usr/bin/env bash
# VERITY GRAPH — offline end-to-end demo (Linux/macOS/WSL/Git Bash).
# Resolves paths from the script location; safe with spaces in paths.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$ROOT_DIR"

BUILD="$ROOT_DIR/build"
VERITY="$BUILD/verity"
VERITY_EXE="$BUILD/verity.exe"
if [ -x "$VERITY_EXE" ]; then VERITY="$VERITY_EXE"; fi
PY="$ROOT_DIR/.venv/Scripts/python.exe"        # Windows venv layout
if [ ! -x "$PY" ]; then PY="$ROOT_DIR/.venv/bin/python"; fi   # POSIX venv

step() { printf '\n== %s ==\n' "$1"; }

step "1. extraction (mock extractor replays gold by design)"
"$PY" -I ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json

step "2. ingest (validation + graph build + entity resolution + timeline check)"
"$VERITY" ingest out/extraction.json

step "3. conflicts (planted findings with derivations + benign explanations)"
"$VERITY" conflicts | head -30

step "4. circular sourcing (D006 -> D007 -> D008 -> D006)"
"$VERITY" circular

step "5. timeline (planted cycle reported, Hasse withheld)"
"$VERITY" timeline | head -6

step "6. key entities + MST backbone"
"$VERITY" keyentities
"$VERITY" backbone | head -12

step "7. binary persistence + indexed random access"
"$VERITY" save out/graph.vg
"$VERITY" load out/graph.vg
"$VERITY" load out/graph.vg --fetch C010

step "8. exports + ledger verification pass"
"$VERITY" export --json out/graph.json --dot out/graph.dot --html out/report.html
"$PY" -I - <<'PY'
import json
with open("out/graph.json", encoding="utf-8") as f:
    report = json.load(f)
with open("out/ledger.json", "w", encoding="utf-8") as f:
    json.dump(report["ledger"], f, indent=2)
print("ledger section written to out/ledger.json")
PY
"$VERITY" verify-ledger out/ledger.json

step "9. ledger tamper FAILS on a disposable copy (pristine file untouched)"
"$PY" -I - <<'PY'
import json, shutil
shutil.copy("out/ledger.json", "out/ledger_tampered.json")
with open("out/ledger_tampered.json", encoding="utf-8") as f:
    ledger = json.load(f)
if "entries_data" not in ledger:
    raise SystemExit("tamper step expects out/ledger.json to exist")
ledger["entries_data"][1] += "|TAMPERED"
with open("out/ledger_tampered.json", "w", encoding="utf-8") as f:
    json.dump(ledger, f)
print("tampered copy written (entry index 1 edited)")
PY
TAMPER_EXIT=0
"$VERITY" verify-ledger out/ledger_tampered.json || TAMPER_EXIT=$?
if [ "$TAMPER_EXIT" -ne 0 ]; then
  echo "expected rejection: tampered ledger verification FAILED with exit $TAMPER_EXIT"
else
  echo "UNEXPECTED: tampered ledger verified — this is a bug"
  exit 1
fi

step "10. DM demonstrations"
"$VERITY" dm sets | head -4
"$VERITY" dm numtheory | head -4
"$VERITY" dm relations | head -3
"$VERITY" dm logic | head -4

step "11. full test suites"
if [ -x "$VERITY_EXE" ]; then "$BUILD/verity_tests.exe" 2>/dev/null | tail -3; else "$BUILD/verity_tests" | tail -3; fi
"$PY" -I -m pytest ai/tests 2>&1 | tail -1

printf '\n== demo complete — all artifacts under out/ ==\n'
