# CONCEPT_MATRIX.md — every required concept with real file + test + demo

Combined from docs/concept_matrix/phase1.md, phase2.md, phase3.md plus the
Phase-4 rows below. Line numbers resolve to the current tree state.

## Phase 4 (persistence, proof, presentation)

| Subject | Unit | Concept | File | Test | Demo |
|---|---|---|---|---|---|
| OOP | U5 | binary serializer with seekg/seekp random access | src/verity/io/GraphSerializer.cpp | tests/io/test_phase4.cpp "indexed random access" | `verity load out/graph.vg --fetch C010` |
| OOP | U5 | overloaded << >> for record I/O | GraphSerializer.cpp (operator<<>>) | exercised by save/load round-trip | `verity save out/graph.vg` |
| OOP | U5 | stream errors -> StorageError + atomic save | GraphSerializer.cpp | "corruption is rejected" (magic/index/truncation) | — |
| OOP | U5 | command-line arguments in main | app/main.cpp | CLI acceptance log | `verity --help` |
| DM | U4 | Miller-Rabin (deterministic witness set) | include/verity/dm/Crypto.hpp | test_phase4 "Miller-Rabin…" | — |
| DM | U4 | CRT with non-coprime/inconsistent rejection | Crypto.hpp (crt) | test_phase4 "CRT…" | — |
| DM | U4 | Fermat check incl. Carmichael honesty | Crypto.hpp (fermatCheck) | test_phase4 "Fermat check" | — |
| DM | U4 | evidence ledger hash chain + verify + tamper rejection | include/verity/dm/Ledger.hpp + engine/Exporter.cpp | test_phase4 "evidence ledger…" | `verity verify-ledger out/ledger.json` |
| DM | U2 | tree law \|E\|=\|V\|-1 + full-binary leaf law | include/verity/ds/Trees.hpp | test_phase4 "tree law…" | — |
| DM | U6 | arithmetic expression tree + traversals | ds/Trees.hpp (ExpressionTree) | test_phase4 "arithmetic expression tree…" | — |
| DM | U6 | spanning-tree verification (Kruskal vs brute oracle) | Trees.hpp (isSpanningTree) | test_phase4 "tree law…" | `verity backbone` |
| DSA | — | adjacency-matrix variant behind same interface | include/verity/ds/GraphMatrix.hpp | test_phase4 "adjacency-matrix variant…" | — |
| DSA | — | 4 benchmark families with real numbers | scripts/bench.cpp | out/validation/phase4_benchmarks_raw.txt | `scripts/bench.sh` |
| Engine | — | exporters json/dot/html (offline standalone) | src/verity/engine/Exporter.cpp | test_phase4 "exporters…" | `verity export` |
| Engine | — | report payload drives the five-tab UI | ui/js/app.js | fixture cross-checks in DM Lab | python -m http.server --directory ui |
| AI | — | D010 citation hub + SAMPLE_DATA reference | data/sample/d010_final_summary.txt | pytest span tests | — |
| AI | — | run_demo offline end to end incl. tamper rejection | scripts/run_demo.sh | out/validation/phase4_demo.log | `scripts/run_demo.sh` |

## Earlier phases

Phase-1 rows: docs/concept_matrix/phase1.md (model, ds, dm, AI, engine/CLI).
Phase-2 rows: docs/concept_matrix/phase2.md (Document family, relations,
functions, combinatorics, algebra, resolver, timeline).
Phase-3 rows: see docs/HANDOFF_PHASE_3.md table (logic, inference,
quantifiers, noisy-OR, graph analysis, conflict strategies, reasoning
engines, DSA algorithms).
