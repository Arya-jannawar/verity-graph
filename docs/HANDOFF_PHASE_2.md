# HANDOFF_PHASE_2.md — Identity & order (owner slot: Arya)

## What exists after Phase 2

Model becomes a full class hierarchy, mentions resolve to canonical entities,
events land on a timeline with a Hasse view, and D004–D005 introduce alias
variants and a planted temporal cycle.

## Public headers and signatures added

| Header | Key API |
|---|---|
| `include/verity/model/Document.hpp` | abstract `Document` + TextDocument/PdfDocument/DocxDocument, `DocumentFactory::create(format, filename, text)`, Composite `TextUnit → Section → Paragraph → Sentence` with code-point offsets |
| `include/verity/model/Evidence.hpp` + `Interfaces.hpp` | `Evidence` abstract root with Supporting/Contradicting/Neutral SIBLINGS, `==`/`<`, IScorable + IExportable interfaces |
| `include/verity/model/Author.hpp` | virtual-inheritance diamond: Person/SourceNode : public virtual Node; Author — exactly ONE Node subobject |
| `include/verity/model/Entity.hpp` (extended) | `operator==`/`<`/`+` (alias union) + `AliasSet` — the proper monoid domain with empty identity |
| `include/verity/model/ClaimNode.hpp` (extended) | `operator>>` (validated confidence), IScorable (score = confidence) |
| `include/verity/dm/Relations.hpp` | `Relation<T>`: all properties, closures incl. Warshall, equivalence classes/partitions, Hasse reduction, lub/glb, lattice, incomparables |
| `include/verity/dm/Functions.hpp` | `FiniteFunction<A,B>`: total/injective/surjective/bijective/compose/inverse |
| `include/verity/dm/Combinatorics.hpp` | own `nextPermutation`, recursive+iterative combinations, guarded permutations, `countLinearExtensions` (n≤9), pigeonhole |
| `include/verity/dm/Algebra.hpp` | semigroup/monoid/group law checkers, `ZnPlus`, `ZpMul`, subgroups, generators, Lagrange, homomorphism checker |
| `include/verity/ds/Trie.hpp` | case/punctuation-normalizing Trie with original-word storage (autocomplete) |
| `include/verity/ds/AVLTree.hpp` | AVL with `between(a,b)` range query, `isBalanced`/`isOrdered` test hooks |
| `include/verity/ds/DisjointSet.hpp` | path compression + union by rank |
| `include/verity/ds/NaryTree.hpp` | pre/post/level order (handwritten Queue), depth/height/leaves |
| `include/verity/ds/Heap.hpp` | MinHeap/MaxHeap + `decreaseKey` + `topK` (bounded heap) |
| `include/verity/ds/TopoSort.hpp` | Kahn + DFS topological sort, directed cycle WITNESS (colors), undirected cycle (DSU) |
| `include/verity/ds/SortAlgo.hpp` | stable mergeSort, deterministic quickSort, binarySearch |
| `include/verity/ds/Levenshtein.hpp` | edit distance + full DP table |
| `include/verity/engine/EntityResolver.hpp` | blocking → alias/initial policy → dm::Relation closure → DSU cross-check → canonical representatives |
| `include/verity/engine/TimelineBuilder.hpp` | AVL date index, strict-order check, Hasse reduction + DOT, topo order, incomparables, "Potential temporal contradiction" + witness |

## What is tested (real output)

- C++ suite: **135 test cases, 144,329 assertions, all passing**
  (`out/validation/phase2_doctest_full.log`).
- ASan: MSVC build, **135/135, no reports** (`out/validation/phase2_asan_msvc.log`).
- Python pytest: **24 passed** (`out/validation/phase2_pytest.log`) — includes
  date-normalization tables, alias-shape detection, rule-extractor span
  exactness, PDF/DOCX reader spans.
- CLI acceptance (`out/validation/phase2_cli_acceptance.log`):
  - ingest runs the resolver: `entity resolution: 6 matched pair(s), 9
    canonical class(es)` with `canonical E005 <- "J. Smith" "John Smith"
    "Mr. Smith" "Smith, John"`;
  - ingest reports the planted cycle: `Potential temporal contradiction:
    witness EV001 EV002 EV003 EV001` and WITHHOLDS the Hasse diagram;
  - `verity query autocomplete smit` works through the Trie.

## Design decisions recorded this phase (details in DECISIONS.md)

1. Alias merge policy: exact token overlap OR same surname + compatible first
   names (identical, initial-compatible, or Levenshtein ≤ 1 typo). Bare
   honorific forms ("Mr. Smith") never auto-merge on the empty first name —
   they merge through gold-recorded contextual aliases. Unrelated same-surname
   people ("Will Smith" vs "John Smith") stay separate.
2. Canonical representative: prefer plain full name over comma/title/initial
   forms, then longer name, then smallest id.
3. `happened_before` claims bind EVENTS (schema + loader + resolver updated
   consistently); all other predicates bind entities.
4. Timeline date semantics: year-precision events get no date-derived order
   edges (only month/day precision does); chronological sort uses expanded
   interval starts (a year sorts at Jan 1 — documented convention).
5. Trie stores original words at leaves; normalization is
   case/punctuation-transparent so "Smith, John" is reachable via "smith".

## Known gaps / notes for the next phase

1. `TimelineResult.finding` is the seed for the Phase-3 TemporalCycleRule.
2. Resolver output is computed at ingest but canonical MERGING (rewiring
   claims to canonical entity ids) lands with Phase 3's conflict grounding.
3. Rule extractor is conservative by design; negation handling and the
   precision/recall report are Phase 3.
4. PDF/DOCX fixtures (d004_format_variant.pdf, d005_format_variant.docx) are
   FORMAT variants of overview content — semantic originals remain d001–d005
   (see SAMPLE_DATA.md when Phase 4 finalizes it).

## Exact commands to reproduce the demo

```bash
scripts/build.sh
./.venv/Scripts/python.exe -I scripts/make_phase2_gold.py
./.venv/Scripts/python.exe -I scripts/make_phase2_fixtures.py
./.venv/Scripts/python.exe -I ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json
./build/verity.exe ingest out/extraction.json     # resolver + timeline output
./build/verity.exe timeline                        # cycle witness, withheld Hasse
./build/verity.exe query autocomplete "smit"
./build/verity_tests.exe                           # 135/135
./.venv/Scripts/python.exe -I -m pytest ai/tests   # 24 passed
```
