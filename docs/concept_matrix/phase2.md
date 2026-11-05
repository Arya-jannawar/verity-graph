# Concept matrix — Phase 2 (Identity & order)

Columns: Subject | Unit | Concept | File | Test | Demo

| Subject | Unit | Concept | File | Test | Demo |
|---|---|---|---|---|---|
| OOP | U3 | Document abstract family + factory | include/verity/model/Document.hpp, src/verity/model/Document.cpp | tests/oop/test_oop2.cpp "DocumentFactory creates…" | — |
| OOP | U3 | Composite (Document>Section>Paragraph>Sentence) | Document.hpp (TextUnit tree) | "Composite tree…" (offsets slice exactly) | — |
| OOP | U3 | Evidence abstract + three SIBLINGS | include/verity/model/Evidence.hpp | test_oop2 "Evidence siblings…" | — |
| OOP | U3 | virtual inheritance diamond, single Node subobject | include/verity/model/Author.hpp | test_oop2 "Author diamond…" | — |
| OOP | U3 | operators ==, <, + (Entity), >> (ClaimNode) | Entity.hpp, ClaimNode.hpp | tests/dm/test_algebra.cpp "Entity operators…", test_oop2 | — |
| OOP | U3 | interfaces IExportable / IScorable | include/verity/model/Interfaces.hpp | test_oop2 "Evidence siblings…", "ClaimNode implements IScorable" | — |
| OOP | U3 | single/multilevel/hierarchical inheritance | Node→Entity; Node→ClaimNode→(IScorable); Entity family | whole suite | — |
| DM | U1 | Relation properties (reflexive/…/strict) | include/verity/dm/Relations.hpp | tests/dm/test_relations.cpp "property checks" | `verity dm relations` |
| DM | U1 | closures incl. Warshall | Relations.hpp | "Warshall closure agrees with brute-force oracle" | — |
| DM | U1 | equivalence classes vs DisjointSet cross-check | Relations.hpp + engine/EntityResolver.cpp | test_relations "…match DisjointSet classes"; resolver test | — |
| DM | U1 | Hasse diagram (transitive reduction) | Relations.hpp (hasseDiagram) | "Hasse reduction removes exactly the transitive edges" | `verity timeline` (DOT withheld on cycle) |
| DM | U1 | minimal/maximal, bounds, lub/glb, lattice | Relations.hpp | "bounds, lub/glb, lattice, incomparables" | `verity dm relations` |
| DM | U1 | partitions (disjoint + covering) | Relations.hpp | "partition checks" | — |
| DM | U1 | functions: injective/surjective/bijective/compose/inverse | include/verity/dm/Functions.hpp | tests/dm/test_functions.cpp | `verity dm functions` |
| DM | U1 | mention→canonical total & onto | engine/EntityResolver.cpp | test_functions "mention-to-canonical…" | `verity ingest …` resolver output |
| DM | U2 | own next-permutation | include/verity/dm/Combinatorics.hpp | tests/dm/test_combinatorics.cpp "nextPermutation matches std" | `verity dm combinatorics` |
| DM | U2 | recursive+iterative combinations | Combinatorics.hpp | "combinations: recursive and iterative agree" | — |
| DM | U2 | linear extensions of a poset | Combinatorics.hpp (countLinearExtensions) | "chain, diamond, antichain" + brute-force oracle | `verity dm combinatorics` |
| DM | U2 | pigeonhole checker | Combinatorics.hpp | "pigeonhole: premise and conclusion" | — |
| DM | U3 | semigroup/monoid/group exhaustive law checkers | include/verity/dm/Algebra.hpp | tests/dm/test_algebra.cpp "(Z_n,+)…", "(Z_p*)…" | `verity dm groups` |
| DM | U3 | subgroups, generators, Lagrange, homomorphism (+ honest fail) | Algebra.hpp | test_algebra subgroup/Lagrange/homomorphism cases | `verity dm groups` |
| DM | U3 | alias-union monoid over AliasSet (empty identity) | include/verity/model/Entity.hpp | test_algebra "AliasSet: … commutative monoid" | — |
| DSA | — | Trie (case-insensitive, prefix, delete) | include/verity/ds/Trie.hpp | tests/ds/test_ds2.cpp + randomized oracle | `verity query autocomplete smit` |
| DSA | — | AVL with between(a,b) | include/verity/ds/AVLTree.hpp | test_ds2 AVL (height 10 @1000, oracle) | — |
| DSA | — | DisjointSet (path compression + rank) | include/verity/ds/DisjointSet.hpp | test_ds2 (brute-force oracle) | — |
| DSA | — | NaryTree traversals | include/verity/ds/NaryTree.hpp | test_ds2 NaryTree | — |
| DSA | — | MinHeap/MaxHeap + decreaseKey + top-K | include/verity/ds/Heap.hpp | test_ds2 Heap/topK vs std::sort | — |
| DSA | — | Kahn + DFS topological sort | include/verity/ds/TopoSort.hpp | tests/ds/test_topo.cpp (random DAG validity) | `verity timeline` |
| DSA | — | directed cycle witness / undirected DSU cycle | TopoSort.hpp | test_topo (self-loop, witness reconstruction) | `verity timeline` (witness path) |
| DSA | — | Levenshtein DP + table | include/verity/ds/Levenshtein.hpp | tests/ds/test_sorts.cpp Levenshtein | — |
| DSA | — | merge sort (stable), quick sort, binary search | include/verity/ds/SortAlgo.hpp | test_sorts oracles | — |
| AI | — | RuleBasedExtractor (entities/dates/aliases/templates) | ai/extractors/rule.py | tests ai/tests/test_phase2.py | `run_extract --extractor rule` |
| AI | — | date normalization table | ai/extractors/rule.py (normalize_date_token) | parametrized date table | — |
| AI | — | alias detection (J./Mr./Surname, First) | ai/extractors/rule.py (detect_aliases) | test_alias_detection_shapes | — |
| AI | — | pdf_reader (pypdf) + docx_reader (python-docx) with spans | ai/readers/*.py | test_pdf/docx_reader_normalized_and_spanned | — |
| Engine | — | EntityResolver (blocking→closure→canonical) | src/verity/engine/EntityResolver.cpp | tests/engine/test_engine2.cpp | `verity ingest` output |
| Engine | — | TimelineBuilder (AVL index, strict order, Hasse, cycle witness) | src/verity/engine/TimelineBuilder.cpp | test_engine2 timeline cases | `verity timeline` |
| Data | — | D004 alias variants + D005 temporal cycle + gold | data/sample/d004…, d005…, data/gold/*.gold.json | pytest span tests; resolver/timeline tests | — |
