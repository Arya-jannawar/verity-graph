# Concept matrix — Phase 1 (Foundation)

Columns: Subject | Unit | Concept | File:lines | Test | Demo command
(Line numbers refer to the state of the repo at this phase boundary.)

| Subject | Unit | Concept | File | Test | Demo |
|---|---|---|---|---|---|
| OOP | U1/U2 | class encapsulation + validated setter | src/verity/model/ClaimNode.cpp (validateConfidence/setConfidence) | tests/oop/test_model.cpp "encapsulation" | `verity_tests.exe -tc=*encapsulation*` |
| OOP | U1/U2 | constructor overloading + default arguments | include/verity/model/ClaimNode.hpp (two ctors) | test_model "constructor overloading" | same |
| OOP | U1/U2 | static members: nextId counters, live-object counter, observeId resume | src/verity/model/Node.cpp | test_model "static nextId", "live-object counter", "observeId" | same |
| OOP | U1/U2 | `this`-pointer chaining | include/verity/model/ClaimNode.hpp (withConfidence…) | test_model "this-pointer chaining" | same |
| OOP | U1/U2 | friend operator<< and friend class | include/verity/model/Node.hpp, GraphInspector.hpp | test_model "friend operator<<", "friend GraphInspector" | same |
| OOP | U1/U2 | const correctness | all model headers (const accessors) | test_model "friend operator<<" (const-ref path) | — |
| OOP | U1/U2 | static vs dynamic binding | tests/oop/test_binding.cpp (BindBase/BindDerived + Node::type) | test_binding | `verity_tests.exe -tc=*binding*` |
| OOP | U2 | Rule of Five raw-ownership store | src/verity/model/NodeStore.cpp | test_nodestore (copy/self-assign/move/leak/ctor-fail) | `verity_tests.exe -tc=*NodeStore*` |
| DM | U4 | gcd, extended Euclid (Bézout) | include/verity/dm/NumTheory.hpp | test_numtheory "extendedGcd: Bezout identity" | `verity dm numtheory` |
| DM | U4 | lcm overflow guard | NumTheory.hpp (lcm) | test_numtheory "lcm … overflow guard" | — |
| DM | U4 | fast modpow, modular inverse | NumTheory.hpp (modPow, modInverse) | test_numtheory "modPow…", "modInverse…" | `verity dm numtheory` |
| DM | U4 | isPrime (trial division), sieve agreement | NumTheory.hpp | test_numtheory "sieve agrees… 0..5000" | — |
| DM | U4 | nextPrime (strictly greater) | NumTheory.hpp | test_numtheory "nextPrime…" + test_hashtable prime capacity | — |
| DM | U4 | Euler totient φ | NumTheory.hpp | test_numtheory "eulerTotient: … multiplicative law" | — |
| DM | U1 | FiniteSet algebra (union/intersection/difference/symmetric difference/subset/equality) | include/verity/dm/FiniteSet.hpp | test_sets "algebra laws" + "randomized oracle vs std::set" | `verity dm sets` |
| DM | U1 | Cartesian product | FiniteSet.hpp (cartesianProduct) | test_sets "cartesian product" | — |
| DM | U1 | guarded power set (2^n, n≤20) | FiniteSet.hpp (powerSet) | test_sets "power set sizes … guard" | — |
| DM | U1 | inclusion–exclusion vs brute force | FiniteSet.hpp (unionSizeInclusionExclusion/…BruteForce) | test_sets "IE identity" (300 random pairs) | `verity dm sets` |
| DM | U2 | factorial overflow guard (21!) | include/verity/dm/Counting.hpp | test_counting "factorial: exact 0..20 … throw at 21" | `verity dm counting` |
| DM | U2 | nCr multiplicative + nPr + overflow | Counting.hpp | test_counting "nCr…", "nPr…" | — |
| DM | U2 | Pascal DP vs multiplicative formula | Counting.hpp (pascalNcr, binomialIdentityCheck) | test_counting "pascalNcr agrees… n<=25" | — |
| DM | U2 | binomial theorem check | Counting.hpp (binomialTheoremCheck) | test_counting "binomial theorem identity" | — |
| DSA | — | LinkedList + iterators + Rule of Five | include/verity/ds/LinkedList.hpp | test_ds_core (std::list oracle, Traced leak counter) | — |
| DSA | — | Stack / Queue | include/verity/ds/Stack.hpp, Queue.hpp | test_ds_core "LIFO…", "FIFO…" + oracle | — |
| DSA | — | HashTable chaining, prime sizing, rehash, string-hash specialization | include/verity/ds/HashTable.hpp | test_hashtable (unordered_map oracle 3×4000 ops; rehash 10k) | — |
| DSA | — | Graph adjacency list (dir/undir, parallel edges, self-loops, degrees, transpose) | include/verity/ds/Graph.hpp | test_graph "degree conventions…", "transpose…" | — |
| DSA | — | BFS k-hop + shortest path | include/verity/ds/GraphAlgo.hpp | test_graph "BFS … vs brute-force oracle" | `verity neighbors C001 --depth 2` |
| DSA | — | DFS recursive == iterative | GraphAlgo.hpp | test_graph "dfsIterative equals dfsRecursive" (30 random graphs) | — |
| AI | — | extraction JSON Schema (draft 2020-12) | ai/schema/extraction.schema.json | pytest test_schema_validates_mock_output | `python -I ai/run_extract.py …` |
| AI | — | predicate vocabulary frozen | data/rules/predicates.json + model/Predicates.hpp | test_jsonloader "predicate vocabulary…" | — |
| AI | — | txt reader normalization contract | ai/readers/txt_reader.py | pytest test_normalization_matches_cpp_contract | — |
| AI | — | mock extractor (gold replay, loud dedupe errors) | ai/extractors/mock.py | pytest test_mock_* | — |
| Engine | — | validated JSON loader (structural + semantic) | src/verity/io/JsonLoader.cpp | test_jsonloader (21 negative paths) | `verity ingest out/extraction.json` |
| Engine | — | typed graph build (7 edge kinds) | src/verity/engine/GraphBuilder.cpp | test_pipeline "…correct edge counts" (13 edges) | — |
| Engine | — | evidence query + BFS neighborhood | src/verity/engine/VerityEngine.cpp | test_pipeline | `verity query evidence "John Smith"` |
| Data | — | D001–D003 neutral + gold | data/sample/*.txt, data/gold/*.gold.json | pytest test_all_gold_spans_exact | — |
