# HANDOFF_PHASE_3.md — Logic & reasoning (owner slot: Rutu)

## What exists after Phase 3

The system reasons: rules parsed as logic formulas are grounded against the
indexed claim set, potential contradictions carry derivations, severity,
exact excerpts and benign explanations; evidence is built through the three
Evidence siblings; provenance is traced with cycles reported; entities and
evidence are ranked; the MST backbone minimizes a documented additive cost.

## Public headers and signatures added

| Header | Key API |
|---|---|
| `include/verity/dm/Logic.hpp` + `src/verity/dm/Logic.cpp` | Formula AST (Var/Const/Not/And/Or/Implies/Iff/Xor), shunting-yard `parseFormula`, prefix/infix/postfix printing, `evaluate`, bounded `truthTable` (≤12 vars), tautology/contradiction/satisfiable, `semanticallyEquivalent`, `toNNF`, `toCNF` (truth-table dual), `toDNF` |
| `include/verity/dm/Inference.hpp` + `.cpp` | definite-Horn `forwardChain` (per-fact derivations), `hypotheticalSyllogism`, `modusTollens` (explicit literals), `disjunctiveSyllogism`, `resolveBinary`/`resolveAll` (guarded 32 clauses) |
| `include/verity/dm/Quantifiers.hpp` | forAll/exists/existsUnique over finite domains + negation-law checks |
| `include/verity/dm/NoisyOr.hpp` | `noisyOrCombine/Aggregate` (monoid over [0,1], identity 0, fp-tolerance), `noisyOrAggregateEvidence` (dedupe by document, circular halved) |
| `include/verity/dm/GraphAnalysis.hpp` | degree sequence, handshake (undirected + digraph), isolated/pendant, components, bipartite 2-coloring, Euler type, guarded Hamiltonian (≤10), induced subgraph, greedy coloring, A^k walk counting |
| `include/verity/ds/GraphAlgorithms.hpp` | Dijkstra (1−confidence cost), Kruskal+Prim (undirected projection, spanning forest), iterative Tarjan SCC, recursive articulation points + bridges (parallel-edge safe) |
| `include/verity/ds/StringSearch.hpp` | Rabin-Karp find/find-all with verified matches (rolling hash mod prime) |
| `include/verity/engine/FixedCache.hpp` | `FixedCache<K,V,N>` non-type-parameter LRU (entity lookups) |
| `include/verity/util/FuncTemplates.hpp` | `countIf`, `intersectSorted` |
| `include/verity/engine/Conflict.hpp` | `Conflict` finding (statement ALWAYS "Potential contradiction"/"Potential temporal contradiction"), Observer `ConflictListener` + CLI printer + JSON collector |
| `include/verity/engine/ConflictRule.hpp` | Strategy base + `DetectionContext` (handwritten-HashTable claim index, FixedCache canonical lookups) |
| `include/verity/engine/ConflictDetector.hpp/.cpp` | detector (runs rules, sorts, assigns X-ids, notifies), `RuleFactory::fromText` frozen DSL, five strategies: FunctionalPredicate / Negation / NumericMismatch / LocationTime / TemporalCycle |
| `include/verity/engine/Reasoning.hpp/.cpp` | `EvidenceBuilder` (Supporting/Contradicting/Neutral siblings), `ProvenanceTracer` (origin tree with retained extra edges, circular groups with witness), `Ranker` (key entities via degree + noisy-OR, top-K evidence, Kruskal backbone) |
| `data/rules/rules.txt` | frozen rule DSL configuration |

## Key semantics frozen this phase

1. DateValue pairs clash only when their precision-preserving intervals are
   DISJOINT (month-vs-day overlap is benign, CONTRACT 10.2).
2. A lone `valid_from` is open-ended ("since 2022-01" = [2022-01-01, ∞)) —
   implemented by overriding the bound AND setting precision to Day so the
   interval expansion cannot wipe the open end (this bug was found by the
   planted CEO-conflict test).
3. Functional rule compares only same-kind, both-Positive object values;
   quantity pairs belong to NumericMismatchRule; negative/unknown polarity
   belongs to NegationRule or is preserved.
4. rules.txt grammar: `functional|negation|numeric|location <pred|any>
   overlap|same_unit`, `temporal_cycle <pred>`; violations throw RuleError
   (never eval/exec).

## What is tested (real output)

- C++ suite: **171 test cases, 145,637 assertions, all passing**
  (`out/validation/phase3_doctest_full.log`) — includes: a NEW ConflictRule
  subclass integrating WITHOUT touching ConflictDetector; benign cases not
  flagged (announced-vs-launch, month-vs-day, restated March); grammar
  violations; Observer delivery to two listeners; exception catch-by-base +
  rethrow; circular sourcing D006→D007→D008→D006 with retained extra edges;
  the intermediary (Elena Vasquez) disconnecting the documented co-mention
  projection over D006–D009.
- ASan: MSVC build **171/171, no reports** (`out/validation/phase3_asan_msvc.log`).
- Python: **39 passed** (`out/validation/phase3_pytest.log`) — negation cues
  (positive/negative/denial), the PUBLISHED scope-leak limitation, LLM
  extractor with a FAKE client (strict JSON, one retry, strict span discard,
  no network), zero-division-guarded precision/recall, mock-labeled report.
- Honest rule-extractor evaluation (rule vs gold): P=0.333 R=0.118 F1=0.174
  with per-predicate rows — printed, not inflated.
- CLI acceptance (`out/validation/phase3_cli_acceptance.log`): 8 findings
  including the CEO conflict C002-vs-C015 (Maria since 2022-01 vs Priya
  since 2023-06 — a genuine overlapping-interval clash), the planted date,
  negation, alibi and numeric conflicts, and the temporal cycle.

## Known gaps / notes for Phase 4

1. `TemporalCycleRule` consumes `TimelineResult.finding`; the ledger and
   binary persistence arrive next.
2. The rule-extractor recall is low BY DESIGN (conservative templates);
   `LIMITATIONS.md` will carry the naive negation-scope and template-coverage
   limitations verbatim.
3. Noisy-OR remains a confidence heuristic; the UI must label it so.

## Exact commands to reproduce the demo

```bash
scripts/build.sh
./.venv/Scripts/python.exe -I scripts/make_phase3_gold.py
./.venv/Scripts/python.exe -I ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json
./build/verity.exe ingest out/extraction.json
./build/verity.exe conflicts          # 8 findings incl. CEO/date/negation/alibi/numeric/cycle
./build/verity.exe circular           # D006 -> D007 -> D008 -> D006
./build/verity.exe keyentities && ./build/verity.exe backbone
./build/verity.exe trace C010 && ./build/verity.exe path C010 D002
./build/verity.exe dm logic && ./build/verity.exe dm graphs
./.venv/Scripts/python.exe -I ai/run_extract.py --input data/sample --extractor rule --out out/extraction_rule.json --evaluate-gold
./build/verity_tests.exe              # 171/171
```
