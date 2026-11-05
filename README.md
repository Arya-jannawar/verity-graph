# VERITY GRAPH

An offline-first **document evidence graph** and **potential-contradiction**
analysis application. Fictional demo data; the system never decides truth —
findings are always labeled **"Potential contradiction"** (or "Potential
temporal contradiction").

Academic project covering OOP (C++), Discrete Mathematics, Data Structures &
Algorithms, and AI (Python extraction), built in four vertical phases
(see `PHASES.md` and `docs/HANDOFF_PHASE_*.md`).

## Status

| Phase | Theme | State |
|---|---|---|
| 1 | Foundation: scaffold, core model, basic structures, thin pipeline | ✅ complete (docs/HANDOFF_PHASE_1.md) |
| 2 | Identity & order: hierarchy, relations, entity resolution, timeline | ⏳ in progress |
| 3 | Logic & reasoning: rules, conflicts, confidence, provenance | planned |
| 4 | Persistence & delivery: binary files, ledger, benchmarks, UI, docs | planned |

## Quick start (Windows)

Prerequisites: CMake ≥ 3.20, Ninja, a C++17 compiler (clang++ or g++; on this
machine LLVM clang is auto-detected), Python 3.13+ with a venv.

```powershell
# build (PowerShell)
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests

# or from Git Bash
scripts/build.sh --tests
```

```bash
# extraction pipeline (offline, mock extractor replays gold by design)
./.venv/Scripts/python.exe -I ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json

# ingest + query
./build/verity.exe ingest out/extraction.json
./build/verity.exe neighbors C001 --depth 2
./build/verity.exe query evidence "John Smith"
./build/verity.exe dm numtheory
```

## Quick start (Linux/macOS/WSL)

```bash
sudo apt install cmake ninja-build g++ python3-venv   # once
python3 -m venv .venv && ./.venv/bin/pip install -r ai/requirements.txt
scripts/build.sh --tests
./build/verity ingest out/extraction.json
```

## Sanitizers

```bash
# Windows (MSVC ASan — full suite incl. exception paths):
#   see scripts/build.ps1 -Sanitize, or via vcvars:
#   cmake -S . -B build-msan -G Ninja -DCMAKE_CXX_COMPILER=cl.exe -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVERITY_SANITIZE=ON
# Linux / WSL (full ASan+UBSan, recommended for complete sanitizer evidence):
sudo apt install g++   # then
scripts/build.sh --sanitize --tests
# valgrind (NodeStore ownership):
sudo apt install valgrind && valgrind --leak-check=full ./build/verity_tests
```

On Windows, LLVM clang's ASan cannot unwind C++ exceptions; use the MSVC ASan
build for sanitizer evidence (verified in `out/validation/phase1_asan_msvc.log`).

## Offline guarantee

All third-party dependencies are vendored (`third_party/`, `ui/vendor/`,
`ai/requirements.txt` pins). Nothing is downloaded at runtime. See
`third_party/THIRD_PARTY.md`.

## Layout

```
include/verity/{model,ds,dm,engine,io,util}   headers (templates header-only)
src/verity/…                                  compiled core
app/main.cpp                                  `verity` CLI
tests/{oop,ds,dm,io,integration}              doctest suite (one runner)
ai/{schema,extractors,readers,tests}          Python extraction pipeline
data/{sample,gold,rules}                      fictional dataset + gold annotations
ui/                                           five-tab frontend (Phase 4)
scripts/                                      build/bench/demo scripts
docs/                                         contract, decisions, status, DM units, handoffs
out/                                          artifacts + validation logs (gitignored except .gitkeep)
```

## Wording guarantee

Every finding the engine emits says **Potential contradiction** (temporal
findings: **Potential temporal contradiction**). The AI never presents a truth
verdict, lie-detector result, or probability of truth. Demo data is fictional
and labeled as such in the UI.
