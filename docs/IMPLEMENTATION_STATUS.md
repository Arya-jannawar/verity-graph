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
