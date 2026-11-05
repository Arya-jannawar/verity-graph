# IMPLEMENTATION_STATUS.md — one row per requirement

Statuses: **Implemented** (code exists) / **Verified** (code exists AND a real
test/log proves it) / **Failed** / **Not run**. Evidence points at logs under
`out/validation/` or a named test case. Updated at each phase boundary.

## Phase 1 — Foundation

| Req | Source | Implementation | Test / evidence | Status |
|---|---|---|---|---|
| Repo layout per SPEC layout; CMake targets verity_ds/dm/core/cli/tests; CLI named `verity` | PHASES P1 Scaffold | `CMakeLists.txt`, tree created | `scripts/build.sh` log `phase1_20261106_024144.txt` | Verified |
| `-DVERITY_SANITIZE=ON` option | PHASES P1 | CMake option + MSVC STL lib discovery | ASan run `phase1_asan_msvc.log` (76/76) | Verified |
| Vendored doctest + nlohmann/json, version records | PHASES P1 | `third_party/`, `THIRD_PARTY.md` | files present; build offline | Verified |
| `docs/CONTRACT.md` (NodeId, enums, Graph iface, schema, predicates) | PHASES P1 + brief §6 | `docs/CONTRACT.md` | frozen before implementation | Verified |
| Exceptions VerityError/GraphError(Dup/Missing)/ParseError | PHASES P1 | `include/verity/util/VerityError.hpp` | tests/oop/test_model, test_nodestore | Verified |
| Logger singleton (only singleton) | PHASES P1 | `verity/util/Logger.hpp/.cpp` | thread-safe Meyers singleton; used by engine | Implemented (unit test in Phase 3 detector use) |
| Node abstract base; Entity/ClaimNode/EventNode/DocumentNode; enum classes | PHASES P1 OOP | `include/verity/model/*.hpp` | tests/oop/test_model, test_binding | Verified |
| Encapsulation: `ClaimNode::setConfidence` throws outside [0,1] | PHASES P1 | `ClaimNode.cpp` | test_model "encapsulation" | Verified |
| Constructor overloading + default args; static nextId C001; live-object counter; chaining; friend << and GraphInspector; const correctness; initializer lists | PHASES P1 | Node/ClaimNode/Entity | test_model (live-object balance, nextId, chaining, operator<<, GraphInspector) | Verified |
| NodeStore Rule of Five (raw new/delete, deep copy, self-assignment, move-from) clean under ASan | PHASES P1 | `NodeStore.hpp/.cpp` | test_nodestore (7 cases); MSVC ASan log | Verified |
| valgrind on NodeStore tests | PHASES P1 acceptance | — | valgrind unavailable on Windows | Not run (Linux command documented) |
| static vs dynamic binding test | PHASES P1 | tests/oop/test_binding.cpp | test_binding | Verified |
| LinkedList/Stack/Queue with iterators + oracle tests | PHASES P1 DSA | `verity/ds/*` | test_ds_core (randomized oracles, fixed seeds) | Verified |
| HashTable separate chaining on own LinkedList, prime sizing via nextPrime, load-factor rehash, polynomial string hash, std::string vs integral hash | PHASES P1 | `verity/ds/HashTable.hpp` | test_hashtable (unordered_map oracle, 10k-element rehash integrity) | Verified |
| Graph adjacency list (dir+undir, weighted, neighbors, degrees, hasEdge, removeEdge, transpose, addEdge overloads, operator[]) | PHASES P1 | `verity/ds/Graph.hpp` | test_graph (degree conventions, parallel edges, self-loops, errors) | Verified |
| BFS k-hop + shortest unweighted path; DFS recursive+iterative | PHASES P1 | `verity/ds/GraphAlgo.hpp` | test_graph (brute-force oracle; iterative==recursive) | Verified |
| Complexity header comments + COMPLEXITY.md first entries | PHASES P1 | every ds/dm header; `docs/COMPLEXITY.md` | file review | Implemented |
| dm numtheory core (gcd, extEuclid, lcm guard, modpow, modinv, isPrime, sieve, nextPrime, totient) | PHASES P1 DM | `verity/dm/NumTheory.hpp` | test_numtheory (Bézout, Fermat, sieve agreement 0..5000) | Verified |
| FiniteSet all ops, Cartesian product, guarded power set, IE vs brute force | PHASES P1 DM | `verity/dm/FiniteSet.hpp` | test_sets (std::set oracle; power set ≤20 guard; IE identity 300 random pairs) | Verified |
| factorial guard, nCr, nPr, Pascal vs formula, binomial check | PHASES P1 DM | `verity/dm/Counting.hpp` | test_counting (grid 0..25 agreement; overflow throws) | Verified |
| extraction.schema.json + predicates.json | PHASES P1 AI | `ai/schema/`, `data/rules/` | schema validated by jsonschema in pytest | Verified |
| BaseExtractor, MockExtractor (loads *.gold.json), txt_reader, run_extract --extractor mock | PHASES P1 AI | `ai/` | pytest 7/7; `run_extract` exit 0 | Verified |
| pytest: schema validity + span correctness | PHASES P1 | `ai/tests/test_extraction.py` | `phase1_pytest.log` (7 passed) | Verified |
| D001–D003 neutral + hand-written gold | PHASES P1 | `data/sample/`, `data/gold/` | span-exactness tests (C++ + Python) | Verified |
| io::JsonLoader validates, throws ParseError, builds graph | PHASES P1 engine | `verity/io/JsonLoader.cpp`, `verity/engine/*` | test_jsonloader (21 negative cases) | Verified |
| CLI skeleton + nonzero exits: ingest, query evidence, neighbors --depth, dm sets|numtheory|counting | PHASES P1 | `app/main.cpp` | CLI acceptance in `phase1_20261106_024144.txt` | Verified |
| Ingest→neighbors C001 --depth 2 prints correct BFS | PHASES P1 acceptance | CLI | logged acceptance run (5-node BFS incl. cross-document D003) | Verified |
| Zero warnings -Wall -Wextra -Wpedantic | PHASES P1 | all sources | build log: 0 warnings (clang 23.1.4) | Verified |
| ASan test run clean | PHASES P1 | MSVC ASan build | `phase1_asan_msvc.log` 76/76 | Verified (Windows/MSVC) |
| UBSan test run clean | PHASES P1 | — | not available on Windows toolchains; Linux commands in README | Not run |

## Phase 2 — Identity & order

| Req | Source | Implementation | Test / evidence | Status |
|---|---|---|---|---|
| Document abstract + Text/Pdf/Docx + factory | PHASES P2 OOP | model/Document.hpp/.cpp | test_oop2 "DocumentFactory…" | Verified |
| Composite TextUnit tree (Document>Section>Paragraph>Sentence) | PHASES P2 | Document.hpp | test_oop2 "Composite tree…" (offsets exact) | Verified |
| Evidence abstract + Supporting/Contradicting/Neutral SIBLINGS | PHASES P2 | model/Evidence.hpp/.cpp | test_oop2 "Evidence siblings…" | Verified |
| Author diamond via virtual inheritance; exactly one Node subobject | PHASES P2 | model/Author.hpp | test_oop2 "Author diamond…" (same Node* both paths) | Verified |
| Operators ==,<,+ on Entity; >> on ClaimNode; functor/IScorable/IExportable | PHASES P2 | Entity.hpp, ClaimNode.hpp, Interfaces.hpp, Evidence.hpp | test_algebra "Entity operators…", test_oop2 | Verified |
| Relations complete (properties, closures incl. Warshall, classes, partitions, Hasse, bounds, lattice, incomparables) | PHASES P2 DM | dm/Relations.hpp | test_relations (Warshall vs brute oracle, Hasse exactness) | Verified |
| Functions: injective/surjective/bijective/composition/inverse + total/onto check | PHASES P2 | dm/Functions.hpp | test_functions | Verified |
| Recursive/iterative combinations, next-permutation, linear extensions, pigeonhole | PHASES P2 | dm/Combinatorics.hpp | test_combinatorics (std oracle, brute-force LE oracle) | Verified |
| Semigroup/Monoid/Group checkers; Z_n, Z_p*; subgroups; generators; Lagrange; homomorphism with honest failure | PHASES P2 | dm/Algebra.hpp | test_algebra | Verified |
| AliasSet monoid (empty identity) per brief §8 | brief §8 | model/Entity.hpp (AliasSet) | test_algebra "AliasSet… commutative monoid" | Verified |
| Trie / AVL(with between) / DSU / NaryTree / Heap(top-K, decreaseKey) | PHASES P2 DSA | ds/Trie,AVLTree,DisjointSet,NaryTree,Heap | test_ds2 (oracles) | Verified |
| Kahn+DFS topo, directed cycle witness, undirected DSU cycle | PHASES P2 | ds/TopoSort.hpp | test_topo (random DAG validity, witness) | Verified |
| Levenshtein (+DP table), merge/quick/binarySearch | PHASES P2 | ds/Levenshtein.hpp, SortAlgo.hpp | test_sorts (std oracles, stability) | Verified |
| RuleBasedExtractor (entities, ISO dates, aliases, templates) | PHASES P2 AI | ai/extractors/rule.py | ai/tests/test_phase2.py | Verified |
| pdf_reader (pypdf), docx_reader (python-docx) with span integrity | PHASES P2 | ai/readers/pdf_reader.py, docx_reader.py | pytest reader tests (fixtures via make_phase2_fixtures.py) | Verified |
| EntityResolver (Trie blocking, Levenshtein, alias table → Relation closure → canonical, DSU cross-check) | PHASES P2 | engine/EntityResolver.cpp | test_engine2 (4 aliases merge; unrelated stay separate) | Verified |
| TimelineBuilder (AVL index, strict order, Hasse DOT, topo, incomparables, cycle → "Potential temporal contradiction") | PHASES P2 | engine/TimelineBuilder.cpp | test_engine2 (cycle witness C007-C009; Hasse withheld) | Verified |
| CLI: timeline, query autocomplete, dm relations/functions/groups/combinatorics | PHASES P2 | app/main.cpp | phase2_cli_acceptance.log | Verified |
| D004 (aliases) + D005 (temporal cycle) with gold | PHASES P2 | data/sample, data/gold | pytest spans; resolver/timeline tests | Verified |
| Zero warnings; ASan clean | PHASES P2 | — | phase2_doctest_full.log (135/135), phase2_asan_msvc.log | Verified |

## Phase 3 — Logic & reasoning

| Req | Source | Implementation | Test / evidence | Status |
|---|---|---|---|---|
| Full exception hierarchy (ExtractionError, StorageError, RuleError) with catch-by-base/derived/rethrow tests | PHASES P3 | util/VerityError.hpp | tests/engine/test_conflicts.cpp "exception hierarchy…" | Verified |
| ConflictRule (Strategy) ×5; adding a rule needs ZERO detector changes; RuleFactory::fromText with frozen grammar | PHASES P3 | engine/ConflictRule.hpp, ConflictDetector.cpp | test_conflicts "a NEW ConflictRule integrates…", "RuleFactory grammar…" | Verified |
| Observer: detector notifies CLI printer + JSON collector | PHASES P3 | Conflict.hpp listeners | test_conflicts "Observer listeners…" | Verified |
| FixedCache<K,V,N> LRU (non-type param) for entity lookups; countIf/intersectSorted templates | PHASES P3 | engine/FixedCache.hpp, util/FuncTemplates.hpp | test_conflicts "FixedCache: LRU…"; used in DetectionContext | Verified |
| Formula AST, shunting-yard parser, prefix/infix/postfix, evaluation, truth tables, tautology/contradiction/satisfiable, equivalences (De Morgan/distributivity/contrapositive/absorption), CNF/DNF bounded | PHASES P3 DM | dm/Logic.hpp/.cpp | tests/dm/test_logic.cpp (12 cases) | Verified |
| Horn forward chaining with derivations; modus tollens/disjunctive syllogism/resolution on EXPLICIT literal representations; hypothetical syllogism | PHASES P3 | dm/Inference.hpp/.cpp | test_logic "inference…" cases | Verified |
| Finite-domain quantifiers + negation laws | PHASES P3 | dm/Quantifiers.hpp | test_logic "quantifiers…" | Verified |
| Noisy-OR monoid ([0,1], combine, 0) via Phase-2 MonoidLaws pattern + order independence + dedup/circular penalty | PHASES P3 | dm/NoisyOr.hpp | test_graphanalysis "noisy-OR…" (fp tolerance) | Verified |
| dm::GraphAnalysis: degree sequence, handshake (incl. digraph), isolated/pendant, bipartite, Euler, guarded Hamiltonian, induced subgraph, coloring, A^k walks | PHASES P3 | dm/GraphAnalysis.hpp | tests/dm/test_graphanalysis.cpp | Verified |
| Dijkstra (1-conf), Kruskal (DSU), Prim, Tarjan SCC, articulation/bridges, Rabin-Karp | PHASES P3 DSA | ds/GraphAlgorithms.hpp, ds/StringSearch.hpp | tests/ds/test_dsa3.cpp (brute-force oracles) | Verified |
| Negation cue detection wired into rule extractor (limitations published) | PHASES P3 AI | ai/extractors/rule.py | pytest test_negation_* incl. published scope leak | Verified |
| LLMExtractor: env config, strict JSON, one retry, strict span discard, fake-client tests, no network | PHASES P3 | ai/extractors/llm.py | pytest test_llm_* | Verified |
| Honest precision/recall report (TP/FP/FN, per-predicate, zero-division guards, mock labeled) | PHASES P3 | ai/extractors/evaluation.py + run_extract --evaluate-gold | pytest + real report (P=0.333 R=0.118) | Verified |
| ConflictDetector grounded through HashTable index; findings with id/rule/claims/docs/spans/derivation/severity/"Potential contradiction"/benign | PHASES P3 | ConflictDetector.cpp | test_conflicts planted-findings case | Verified |
| EvidenceBuilder (claim↔doc, three siblings) | PHASES P3 | engine/Reasoning.cpp | exercised by top-evidence + tests | Verified |
| ProvenanceTracer (origin tree, circular via cycle detection, extra edges retained) | PHASES P3 | Reasoning.cpp | test_conflicts "circular sourcing…" | Verified |
| Ranker (heap + noisy-OR), keyentities/backbone/path/top-evidence/trace CLI | PHASES P3 | Reasoning.cpp + main.cpp | phase3_cli_acceptance.log | Verified |
| D006–D009 planted: date conflict, CEO conflict, alibi, circular sourcing, benign near-conflicts, intermediary | PHASES P3 | data/sample/d006-d009 + gold | planted-findings + benign + intermediary tests | Verified |
| Zero warnings; ASan clean; pytest green | PHASES P3 | — | phase3_doctest_full.log (171/171), phase3_asan_msvc.log, phase3_pytest.log (39) | Verified |
