# HANDOFF_PHASE_1.md — Foundation (owner slot: Raj)

## What exists after Phase 1

Thin but complete vertical slice: mock extraction JSON → validated ingest →
typed in-memory evidence graph → CLI queries. All Phase-1 scope from
PHASES.md is implemented: scaffold, contract, OOP Units 1–2, DM (numtheory
core, sets, counting basics), DSA (list/stack/queue/hashtable/graph + BFS/DFS),
AI (schema, mock extractor, txt reader), engine/CLI skeleton, D001–D003.

## Public headers and signatures added

| Header | Key API |
|---|---|
| `include/verity/util/VerityError.hpp` | `VerityError`, `ParseError`, `GraphError`, `DuplicateNodeError`, `MissingNodeError` (full hierarchy incl. later-phase classes declared) |
| `include/verity/util/Logger.hpp` | Meyers-singleton `Logger` (debug/info/warn/error, optional file sink) — the only singleton |
| `include/verity/model/NodeId.hpp` | strong `NodeId`, canonicalizes to padded form, `std::hash` specialization |
| `include/verity/model/Enums.hpp` | `EntityType`, `RelationType`, `Polarity`, `EvidenceKind`, `DocumentFormat` + to/from strings |
| `include/verity/model/Node.hpp` | abstract `Node` (type/describe/toJson/clone), static `nextId`, `observeId`, `liveObjects`, friend `operator<<` + `GraphInspector` |
| `include/verity/model/Entity.hpp` | `Entity(name, EntityType, aliases, id)` |
| `include/verity/model/ClaimNode.hpp` | `ClaimNode` (validated `setConfidence`, chaining `withConfidence/withPolarity/withValidTime`, `ClaimObject`) |
| `include/verity/model/EventNode.hpp` | `EventNode(label, entity, DateInterval, doc, span)` |
| `include/verity/model/DocumentNode.hpp` | `DocumentNode(filename, format, normalized_text, citations)` |
| `include/verity/model/NodeStore.hpp` | Rule-of-Five owning store (`add(unique_ptr<Node>)`, `find`, `findAs<T>`) |
| `include/verity/model/Extraction.hpp` | `Span`, `DatePoint/DateInterval` (precision-preserving, `overlaps`/`strictlyBefore`), `ClaimObject`, extraction record structs, `normalizeText`, UTF-8 code-point helpers |
| `include/verity/model/Predicates.hpp` | frozen 7-predicate vocabulary |
| `include/verity/ds/LinkedList.hpp` | template list + iterators, Rule of Five |
| `include/verity/ds/Stack.hpp` / `Queue.hpp` | composed adapters on the list |
| `include/verity/ds/HashTable.hpp` | separate chaining, prime sizing (`dm::nextPrime`), polynomial string hash, rehash, iterators |
| `include/verity/ds/Graph.hpp` | `Graph<N,E>` per CONTRACT §5 (directed/undirected, parallel edges, self-loops, degree conventions, transpose, `Edge` overload) |
| `include/verity/ds/GraphAlgo.hpp` | `bfsKHop`, `bfsShortestPath`, `dfsRecursive`, `dfsIterative` |
| `include/verity/dm/NumTheory.hpp` | gcd, extendedGcd, lcm (guarded), modPow, modInverse, isPrime, sieve, nextPrime, eulerTotient |
| `include/verity/dm/FiniteSet.hpp` | `FiniteSet<T>` algebra + `powerSet` (n≤20 guard) + IE functions |
| `include/verity/dm/Counting.hpp` | factorial (n<21), nCr/nPr (guarded), `pascalNcr`, `binomialIdentityCheck`, `binomialTheoremCheck` |
| `include/verity/io/JsonLoader.hpp` | `loadExtractionFile` / `parseExtraction` — structural + semantic validation |
| `include/verity/engine/GraphBuilder.hpp` | `EdgeAttr`, `EvidenceGraph`, `GraphBuilder::build` |
| `include/verity/engine/VerityEngine.hpp` | facade: `ingest`, `neighbors(id, depth)`, `queryEvidence(name)` |

## What is tested (real output)

- C++ suite: **76 test cases, 130,964 assertions, all passing**
  (`out/validation/phase1_doctest_full.log`):
  ```
  [doctest] test cases:     76 |     76 passed | 0 failed | 0 skipped
  [doctest] assertions: 130964 | 130964 passed |  0 failed |
  [doctest] Status: SUCCESS!
  ```
- Python pytest: **7 passed** (`out/validation/phase1_pytest.log`).
- Build: clang 23.1.4, `-Wall -Wextra -Wpedantic`, **0 warnings**
  (`out/validation/phase1_20261106_024144.txt`).
- ASan: MSVC `cl.exe /fsanitize=address` run, **76/76 passing, no reports**
  (`out/validation/phase1_asan_msvc.log`). LLVM-clang ASan on Windows cannot
  unwind C++ exceptions (limitation log kept at
  `phase1_asan_clang_windows_limitation.log`).
- UBSan / valgrind: **Not run** (unavailable on this Windows host; exact Linux
  commands are in README.md).
- CLI acceptance (same log): ingest of the Python-produced `out/extraction.json`
  (3 documents, 9 entities, 6 claims, 2 citations → 18 nodes, 33 edges),
  `neighbors C001 --depth 2` (correct 5-node BFS incl. cross-document D003 via
  mention edges), `query evidence "John Smith"` (exact quote incl. "Café
  Meridian"), `dm numtheory`.

## Known gaps / notes for the next phase

1. IDs: counters resume via `Node::observeId` — Phase 2 entity resolution may
   mint mention entities; use `nextId("E")` after ingest and it will not collide.
2. `Graph::neighbors()` returns out-edges only; BFS follows out-edges (Cites
   direction D003→D001 is respected). In-edges available via `inNeighbors`.
3. `MockExtractor` is the only extractor; rule-based extractor, alias handling
   and date normalization arrive in Phase 2.
4. Events array is empty by design for D001–D003 (TimelineBuilder is Phase 2).
5. Windows: build via `scripts/build.sh` (Git Bash) or `scripts/build.ps1`;
   clang is auto-detected, `llvm-rc.exe` is wired for the resource compiler.

## Exact commands to reproduce the demo

```bash
scripts/build.sh
./.venv/Scripts/python.exe -I ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json
./build/verity.exe ingest out/extraction.json
./build/verity.exe neighbors C001 --depth 2
./build/verity.exe query evidence "John Smith"
./build/verity.exe dm numtheory
./build/verity_tests.exe        # 76/76
```
