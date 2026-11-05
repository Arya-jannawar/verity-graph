# VERITY GRAPH: 4-Person Phase Plan

> Save this next to `SPEC.md` in the repo root. `SPEC.md` stays the single source of truth for *what* to build.
> This file decides *who builds which slice, in what order*. It **replaces the "Phases 0 to 8" list in SPEC.md**.
> Each phase is a **vertical slice**: every owner touches OOP, Discrete Mathematics, DSA, the AI module and the engine/CLI, so everyone learns all subjects and every phase ends with something that runs.

---

## 1. Who owns what

| Phase | Owner | Theme | Ends with a demo of |
|---|---|---|---|
| **1** | **Raj** | Foundation: scaffold, core model, basic structures, thin end-to-end pipeline | `ingest` a mock extraction, then `neighbors` / `query evidence` |
| **2** | **Arya** | Inheritance, ordering and identity: hierarchy, relations, entity resolution, timeline | `timeline` with Hasse diagram, alias resolution |
| **3** | **Rutu** | Logic and reasoning: rules, conflicts, confidence, provenance | `conflicts`, `circular`, `keyentities`, `backbone`, `trace` |
| **4** | **Abhi** | Persistence, proof and presentation: binary files, ledger, benchmarks, UI, docs | `scripts/run_demo.sh` offline end to end, five-tab UI |

Order is sequential: **Raj, then Arya, then Rutu, then Abhi**. Each person branches from the previous person's merged work.

## 2. Coverage matrix (every subject appears in every phase)

| Subject | Phase 1 (Raj) | Phase 2 (Arya) | Phase 3 (Rutu) | Phase 4 (Abhi) |
|---|---|---|---|---|
| **OOP** | Units 1 and 2: classes, encapsulation, constructors, Rule of Five, static, `this`, friend, enums, binding | Unit 3: inheritance, diamond, polymorphism, operators, interfaces, Composite, Factory | Unit 4: exceptions, templates, Strategy, Observer, Singleton use | Unit 5: files, streams, binary serialization with `seekg/seekp`, CLI args |
| **DM** | Unit 4 core (gcd, modpow, primes), Unit 1 sets and inclusion-exclusion, Unit 2 counting basics | Unit 1 relations, functions, Unit 2 linear extensions and permutations, Unit 3 generic checkers and groups | Unit 1 logic, predicate logic, inference, Unit 3 noisy-OR, Unit 5 graph theory | Unit 4 rest (Miller-Rabin, CRT, ledger chain), Unit 2 recurrences and induction, Unit 6 trees, `docs/dm/*` |
| **DSA** | List, Stack, Queue, HashTable, Graph (adjacency list), BFS, DFS | Trie, AVL, DSU, N-ary tree, Heap, topological sort, Levenshtein, sorting, binary search | Dijkstra, Kruskal, Prim, Tarjan (SCC, cut vertices, bridges), Rabin-Karp, top-K | Adjacency-matrix variant, all four benchmarks, `COMPLEXITY.md` |
| **AI** | Schema, predicate vocabulary, mock extractor, txt reader | Rule-based extractor (entities, dates, aliases, claim templates), pdf and docx readers | Negation detection, LLM extractor, precision/recall report | Final dataset checks, `run_extract` end to end, offline run |
| **Engine / CLI / UI** | JSON loader, graph build, CLI skeleton | `EntityResolver`, `TimelineBuilder`, `timeline` command | `ConflictDetector`, `EvidenceBuilder`, `ProvenanceTracer`, `Ranker`, six CLI commands | Ledger, exporters, `bench`, full CLI, five-tab UI, `run_demo.sh` |
| **Sample data** | D001 to D003 (neutral base documents) | D004, D005 (aliases, temporal cycle) | D006 to D009 (conflicts, circular sourcing, intermediary, benign case) | D010, docx and pdf generation, `SAMPLE_DATA.md` |
| **Docs** | Concept matrix rows, `HANDOFF_PHASE_1.md` | Rows, handoff 2 | Rows, handoff 3 | Merge all rows, `ARCHITECTURE`, `CLASS_DIAGRAM`, `VIVA`, `LIMITATIONS` |

## 3. Rules that apply to every phase

1. **Branch per phase:** `phase-1-foundation`, `phase-2-ordering`, `phase-3-reasoning`, `phase-4-delivery`. Merge to `main` only after the review checklist passes.
2. **Stay in scope.** Build only what your phase lists. If you need a small addition in an earlier phase's file, make it minimal and additive, and mention it in your handoff note.
3. **Handoff note:** end every phase by writing `docs/HANDOFF_PHASE_N.md` with: public headers and signatures added, what is tested, known gaps, and the exact commands to reproduce your demo.
4. **Concept matrix without merge conflicts:** each phase writes its own rows to `docs/concept_matrix/phaseN.md` (columns: Subject | Unit | Concept | File:lines | Test | Demo command). Abhi merges them into `docs/CONCEPT_MATRIX.md`.
5. **Definition of green** for every phase: builds with `-Wall -Wextra -Wpedantic` and zero warnings, ASan and UBSan test run is clean, `pytest` passes, no `TODO`, no stubs, real test output pasted into the handoff note, never delete or weaken a test.
6. **Process inside Antigravity:** produce a plan and task list first, wait for approval, then build, test, show real output, commit, stop for review.
7. **Wording:** always "Potential contradiction". The AI never decides truth.

### Day-1 contract (all four agree before anyone starts)
So work can overlap safely, freeze these early in `docs/CONTRACT.md`: the `NodeId` type, `RelationType` and `EntityType` enums, the `ds::Graph` public interface, the JSON schema, and the predicate list. Changing them later needs everyone's agreement.

---

# PHASE 1: RAJ (Foundation)

**Goal:** a thin but complete vertical slice. Mock extraction JSON goes in, an in-memory graph is built, and the CLI can query it.
**Starts from:** an empty repo with `SPEC.md` and this file.

## Paste this into Antigravity

```
Read SPEC.md and PHASES.md completely. You are doing PHASE 1 ONLY (owner: Raj).
Do not write code yet. First produce an implementation plan, a task list, the file tree for this phase,
and any ambiguities. Wait for my approval. Then build, test with real output, commit on branch
phase-1-foundation, write docs/HANDOFF_PHASE_1.md and docs/concept_matrix/phase1.md, and stop.

SCOPE

Scaffold
- Repository layout exactly as in SPEC.md. CMake targets: verity_ds, verity_dm, verity_core, verity_cli, tests.
- -DVERITY_SANITIZE=ON option, scripts/build.sh, vendored doctest/Catch2 and nlohmann/json, README skeleton.
- docs/CONTRACT.md (NodeId, enums, Graph interface, schema, predicate list).
- Exceptions needed now: VerityError, GraphError (DuplicateNodeError, MissingNodeError), ParseError.
- Logger (Singleton, the only singleton in the project).

OOP (Units 1 and 2, complete)
- Node (abstract, virtual base), Entity (Person, Organization, Product, Location), ClaimNode, EventNode, DocumentNode.
- enum class: EntityType, RelationType, Polarity, EvidenceKind. Namespaces as specified.
- Encapsulation with validated setters (ClaimNode::setConfidence throws outside [0,1]).
- Constructor overloading and default arguments; static nextId() (C001...) and live-object counter;
  this-pointer method chaining; friend operator<< and GraphInspector; const correctness; initializer lists.
- NodeStore: raw new/delete, copy ctor, copy assignment, destructor, move ctor, move assignment (Rule of Five).
  Must be clean under ASan and valgrind.
- tests/oop/test_binding.cpp showing static vs dynamic binding.
- Do NOT yet write Document hierarchy, Evidence family, Author diamond, operator overload set beyond <<, or Strategy rules (Phase 2 and 3).

DM
- Unit 4 core in dm/numtheory: gcd, extended Euclid, lcm, fast modpow, modular inverse, isPrime (trial division),
  sieve, nextPrime, Euler totient. (nextPrime is needed by HashTable.)
- Unit 1 sets: FiniteSet<T> (bitset-backed) with all set operations, Cartesian product, guarded power set,
  inclusion-exclusion unionSizeIE vs brute force.
- Unit 2 basics: factorial with overflow guard, nCr, nPr, Pascal DP vs multiplicative formula, binomial theorem check.
- Docs: docs/dm/UNIT_4.md (partial: core section), docs/dm/UNIT_1.md (sets section), UNIT_2.md (counting section).

DSA (hand-written, tested against std oracles with randomized inputs)
- LinkedList<T> with iterator, Stack<T>, Queue<T>.
- HashTable<K,V>: separate chaining on your LinkedList, prime sizing via dm::nextPrime, load-factor rehash,
  polynomial string hash; template specialization for std::string vs integral keys.
- Graph<N,E>: adjacency list, directed and undirected, weighted, neighbors, in/outDegree, hasEdge, removeEdge,
  transpose, addEdge overloads, operator[].
- BFS (k-hop and shortest unweighted path), DFS (recursive and iterative).
- Complexity header comment on every item and the first entries of docs/COMPLEXITY.md.

AI (Python)
- ai/schema/extraction.schema.json and data/rules/predicates.json exactly as in SPEC.md.
- BaseExtractor, MockExtractor (loads *.gold.json), txt_reader, run_extract.py --extractor mock.
- pytest: schema validity and span correctness.

Sample data
- D001 to D003 (txt) about the two fictional companies, with hand-written gold JSON. Neutral content: entities, products,
  a few claims, no planted conflicts yet.

Engine and CLI
- io::JsonLoader (validates, throws ParseError), builds ds::Graph with model nodes and typed edges.
- CLI skeleton with command parser and non-zero exit codes. Commands: ingest (in-memory, no binary save yet),
  query evidence <name>, neighbors <id> --depth k, dm <sets|numtheory|counting>.

ACCEPTANCE
- scripts/build.sh passes with zero warnings; ASan/UBSan test run clean; valgrind clean on NodeStore tests.
- python ai/run_extract.py --input data/sample --extractor mock --out out/extraction.json passes schema validation.
- verity ingest out/extraction.json && verity neighbors C001 --depth 2 prints a correct BFS neighborhood.
- Every DS structure has a randomized oracle test; every DM law (Pascal vs formula, IE vs brute force) is tested.
```

---

# PHASE 2: ARYA (Ordering and identity)

**Goal:** the model becomes a real class hierarchy, mentions are resolved to canonical entities, and events are placed on a timeline.
**Starts from:** Raj's merged `main` and `docs/HANDOFF_PHASE_1.md`.

## Paste this into Antigravity

```
Read SPEC.md, PHASES.md and docs/HANDOFF_PHASE_1.md completely. You are doing PHASE 2 ONLY (owner: Arya).
Do not write code yet. First produce a plan, task list, file tree for this phase and ambiguities. Wait for approval.
Then build, test with real output, commit on branch phase-2-ordering, write docs/HANDOFF_PHASE_2.md and
docs/concept_matrix/phase2.md, and stop.

SCOPE

OOP (Unit 3, complete)
- Document (abstract) with TextDocument, PdfDocument, DocxDocument. DocumentFactory::create(type, path).
- Composite: TextUnit tree Document > Section > Paragraph > Sentence.
- Evidence (abstract) with SupportingEvidence, ContradictingEvidence, NeutralEvidence as SIBLINGS.
- SourceNode and Author : public Person, public SourceNode using VIRTUAL inheritance, with a test proving a single Node subobject.
- Pure virtuals (Node::describe, Document::sections), virtual destructors everywhere, override/final used correctly.
- Operators: == and < on Entity and Evidence, + on Entity (alias union), >> on ClaimNode, operator() functor comparator for heaps.
- Interfaces IExportable (toJson, toDot) and IScorable (score). Function overloading Graph::addEdge(Edge).
- Single, multilevel and hierarchical inheritance all visible in the hierarchy.

DM
- Unit 1 relations, complete: Relation<T> as boolean matrix; reflexive, irreflexive, symmetric, antisymmetric, asymmetric,
  transitive; isEquivalence, isPartialOrder, isStrictOrder; inverse, composition, union, intersection, complement;
  closures including Warshall; equivalence classes and partitions (disjoint and covering check);
  Hasse diagram, minimal/maximal, bounds, lub/glb, isLattice, comparable vs incomparable pairs.
- Unit 1 functions: injective, surjective, bijective, composition, inverse. Verify mention -> canonical entity is total and onto.
- Unit 2: recursive and iterative combinations and permutations (next-permutation), count linear extensions of a small poset,
  pigeonhole checker.
- Unit 3 (generic part): Semigroup<T>, Monoid<T>, Group<T> exhaustive law checkers; groups (Z_n,+) and (Z_p*,x),
  subgroup check, cyclic generators, Lagrange's theorem on small groups, homomorphism checker with an honest failing example.
  (Noisy-OR is Phase 3.) Add Entity merge (+) as a monoid test.
- Docs: UNIT_1.md (relations, functions), UNIT_2.md (permutations, linear extensions), UNIT_3.md (groups part).

DSA
- Trie (insert, search, prefix, delete, case-insensitive), AVLTree with between(a,b) range query, DisjointSet (path compression and rank),
  NaryTree (pre/post/level order, depth, height, leaves), MinHeap/MaxHeap with decreaseKey and top-K helper.
- Topological sort (Kahn and DFS), DFS edge classification and cycle detection for directed (colors) and undirected (DSU) graphs.
- Levenshtein DP (include a test that prints the DP table), merge sort, quick sort, binary search.
- Oracle tests with randomized inputs; update COMPLEXITY.md.

AI (Python)
- RuleBasedExtractor: capitalized-phrase entities, ISO date normalization (YYYY, YYYY-MM, YYYY-MM-DD, ranges), alias detection
  ("J. Smith", "Mr. Smith", "Smith, John"), regex templates for the starter predicates. spaCy optional, must degrade gracefully.
- pdf_reader (pypdf) and docx_reader (python-docx). Document tree splitting with correct character offsets.
- pytest: date normalization table tests, span correctness.
- Do NOT do negation, LLM extractor or precision/recall yet (Phase 3).

Sample data
- D004 and D005 with gold JSON: alias variants of John Smith, and a temporal cycle in "before" claims
  (planted cases 5 and 7). Do not remove or change Raj's files.

Engine and CLI
- EntityResolver: Trie blocking, Levenshtein, alias table, similarity pairs -> dm::Relation -> equivalence closure -> canonical entities.
  Cross-check against DisjointSet in a test.
- TimelineBuilder: AVL date index, HappenedBefore as a strict partial order, Hasse diagram (DOT), topological order,
  incomparable event list, a cycle reported as "Potential temporal contradiction".
- CLI: timeline, query autocomplete using the Trie, dm relations, dm sets.
- Keep using the Phase 1 loader; ingest now runs the resolver and timeline.

ACCEPTANCE
- All four alias variants of John Smith resolve to one canonical entity; partition checks pass.
- verity timeline prints the order, incomparable events and a DOT Hasse diagram; the temporal cycle in D005 is reported.
- A test proves Author has exactly one Node subobject.
- Build is warning-free; sanitizers clean; pytest green.
```

---

# PHASE 3: RUTU (Logic and reasoning)

**Goal:** the system can reason. Rules written as logic formulas are parsed, evaluated and turned into potential contradictions with derivations. Evidence is scored and sources are traced.
**Starts from:** Arya's merged `main` and `docs/HANDOFF_PHASE_2.md`.

## Paste this into Antigravity

```
Read SPEC.md, PHASES.md and docs/HANDOFF_PHASE_1.md and HANDOFF_PHASE_2.md completely. You are doing PHASE 3 ONLY (owner: Rutu).
Do not write code yet. First produce a plan, task list, file tree for this phase and ambiguities. Wait for approval.
Then build, test with real output, commit on branch phase-3-reasoning, write docs/HANDOFF_PHASE_3.md and
docs/concept_matrix/phase3.md, and stop.

SCOPE

OOP (Unit 4, complete)
- Complete the exception hierarchy: ExtractionError, StorageError, RuleError (and GraphError children already exist).
  Tests: catch by base and derived, multiple catch blocks, rethrow with throw;, exception from a constructor with no leak
  (document file missing), and a note on never throwing from destructors.
- ConflictRule (Strategy) with FunctionalPredicateRule, NegationRule, NumericMismatchRule, LocationTimeRule, TemporalCycleRule.
  Adding a rule must need zero changes in ConflictDetector. RuleFactory::fromText(line).
- Observer: ConflictDetector notifies subscribed listeners (CLI printer, JSON collector).
- Templates: FixedCache<K,V,N> (non-type parameter, LRU) used for entity lookups; function templates countIf and intersectSorted.
- Use Logger (from Phase 1) in the detector.

DM
- Unit 1 logic, complete: Formula expression tree (Var, Const, Not, And, Or, Implies, Iff, Xor); shunting-yard parser;
  prefix/infix/postfix printing; evaluation; truth tables; tautology/contradiction/satisfiable; equivalence checks
  (De Morgan, distributivity, contrapositive, absorption); CNF and DNF.
- Inference engine: forward chaining over Horn clauses with modus ponens, modus tollens, hypothetical syllogism,
  disjunctive syllogism, resolution. Every inferred fact records the rule and premises.
- Predicate logic: finite-domain forall/exists, nested quantifiers, negation of quantifiers verified over test domains.
- Unit 3 (applied part): noisy-OR combine(a,b) = 1-(1-a)(1-b). Test that ([0,1], combine) is a commutative monoid with identity 0
  using Arya's Monoid checker, and that merge order never changes the result.
- Unit 5: dm::GraphAnalysis over ds::Graph: degree sequence, handshake theorem (including digraph in/out), isolated and pendant vertices,
  walk/trail/path/cycle, connectedness, bipartite test (BFS 2-coloring), Euler existence, Hamiltonian brute force for <= 10 vertices,
  subgraph, induced subgraph, union, intersection, complement, isomorphism (small), greedy coloring, A^k walk counting.
- Docs: UNIT_1.md (logic part), UNIT_3.md (noisy-OR part), UNIT_5.md.

DSA
- Dijkstra with MinHeap (edge weight 1 - confidence), Kruskal (with DSU), Prim, Tarjan SCC, articulation points and bridges,
  connected components, Rabin-Karp using a dm rolling hash mod prime, top-K evidence using the heap.
- Oracle and randomized tests; update COMPLEXITY.md.

AI (Python)
- Negation cue detection ("did not", "denied") and polarity, wired into RuleBasedExtractor.
- LLMExtractor: provider and key from environment variables, strict JSON-only prompt, schema validation, one retry,
  discard any claim whose span is not found in the source text.
- Precision/recall report for rule vs gold on all current sample documents. Print the numbers honestly.
- pytest for negation and the span-discard behavior (use a fake LLM client, no network).

Sample data
- D006 to D009 with gold JSON, planting: date conflict (Product X March vs June 2024), CEO conflict, alibi conflict,
  circular sourcing (D6 cites D7, D7 cites D8, D8 cites D6), a benign near-conflict (announced vs launched, month vs full date),
  and a key intermediary entity whose removal disconnects the network.

Engine and CLI
- data/rules/rules.txt parsed into Formula trees; ConflictDetector grounds and evaluates them, indexed by subject + predicate
  through the HashTable; every conflict has id, rule, two claims, documents and spans, derivation, severity, the text
  "Potential contradiction" and a "possible benign explanation" field.
- EvidenceBuilder (claim <-> document links, the three Evidence siblings), ProvenanceTracer (citation graph, origin tree,
  circular sourcing via cycle detection), Ranker (heap + noisy-OR).
- CLI: conflicts, circular, keyentities, backbone (MST), path (Dijkstra), top-evidence, trace, dm logic, dm algebra, dm graphs.

ACCEPTANCE
- verity conflicts lists the planted date, CEO and alibi conflicts with derivations, and does NOT flag the benign case strongly.
- verity circular reports the D6 -> D7 -> D8 -> D6 cycle.
- verity keyentities reports the planted intermediary.
- A test proves a new ConflictRule subclass works without editing ConflictDetector.
- Truth tables, handshake checks and monoid law checks have passing tests; build clean; sanitizers clean; pytest green.
```

---

# PHASE 4: ABHI (Persistence, proof and presentation)

**Goal:** make the project durable, measurable, visible and defensible. Binary files with random access, tamper-evident ledger, benchmarks, UI and the documents an examiner reads.
**Starts from:** Rutu's merged `main` and `docs/HANDOFF_PHASE_3.md`.

## Paste this into Antigravity

```
Read SPEC.md, PHASES.md and all docs/HANDOFF_PHASE_*.md completely. You are doing PHASE 4 ONLY (owner: Abhi).
Do not write code yet. First produce a plan, task list, file tree for this phase and ambiguities. Wait for approval.
Then build, test with real output, commit on branch phase-4-delivery, write docs/HANDOFF_PHASE_4.md, and stop.

SCOPE

OOP (Unit 5, complete)
- GraphSerializer::save/load in binary mode: header (magic number, version), records, offset index at the end,
  random access with seekg/seekp to fetch one record by id without reading the whole file.
- Overloaded << and >> used for record I/O. Text reading with ifstream and stringstream where relevant.
- Stream error handling (fail, eof, bad) converted to StorageError. Command-line arguments in main.
- Tests: round-trip equality, random-access fetch, corrupted-file handling, no leaks.
- CLI: verity ingest now saves out/graph.vg; verity load out/graph.vg.

DM
- Unit 4 remainder: Miller-Rabin, Chinese Remainder Theorem, Fermat check, and the evidence ledger hash chain
  h_i = H(record_i, h_{i-1}) with a modular hash. verify-ledger passes on clean data and FAILS after a deliberate edit.
  State clearly that this is teaching-grade, not SHA-256. Optional: toy RSA signing the ledger head.
- Unit 2 remainder: recurrences (Fibonacci-like reachable growth, T(n)=2T(n/2)+n, pairwise comparison count) printed with
  recurrence, closed form and measured counts; induction proofs in docs/dm/UNIT_2.md for sum of degrees = 2|E|,
  tree on n vertices has n-1 edges, BFS layering correctness; random-graph tests (thousands) for each.
- Unit 6 trees: tree property checks (|E| = |V|-1, full binary tree with i internal nodes has i+1 leaves),
  arithmetic expression tree with traversals, spanning-tree verification of Kruskal vs Prim vs brute force on tiny graphs,
  provenance origin-tree verification, document-tree traversals.
- Finish docs/dm/UNIT_1.md to UNIT_6.md so each has definitions, theorem, use in Verity, worked example, test names.
- CLI: dm trees, dm all.

DSA
- Adjacency-matrix variant of Graph behind the same interface.
- scripts/bench.sh and docs/BENCHMARKS.md with REAL numbers: (1) list vs matrix at 1k, 5k, 10k nodes, (2) HashTable vs
  std::unordered_map at 100k keys, (3) merge vs quick vs std::sort, (4) naive pairwise vs indexed conflict detection
  reporting comparison counts. Never invent numbers.
- Finalize docs/COMPLEXITY.md so every structure and algorithm has an entry.

AI (Python)
- D010, scripts to generate the docx and pdf sample documents, and docs/SAMPLE_DATA.md listing every planted case with the
  expected output of each CLI command.
- run_extract.py works for rule, llm and mock end to end; the full demo runs offline.
- Re-run and record the final precision/recall numbers.

Engine, UI and demo
- Exporters: verity export --json out/graph.json --dot out/graph.dot --html out/report.html. verity bench.
- ui/ with vis-network (local copy): Graph, Conflicts, Timeline, Provenance and DM Lab tabs reading out/graph.json,
  using the wording "Potential contradiction" everywhere.
- scripts/run_demo.sh prints: extracted counts, all planted conflicts with derivations, the circular-sourcing cycle,
  the timeline with Hasse diagram, key entities, MST backbone, a ledger verification pass, and a ledger verification failure
  after a record is deliberately edited.

Documentation
- Merge docs/concept_matrix/phase1-3.md plus your own into docs/CONCEPT_MATRIX.md and check that every concept in SPEC.md
  has a row with real file:lines and a passing test.
- README.md, ARCHITECTURE.md (Mermaid), CLASS_DIAGRAM.md (matching real code), VIVA.md (10 questions per module with short answers
  plus a 5-minute demo script), LIMITATIONS.md.

OPTIONAL (only if everything above is green): SPEC.md Phase 8, SQLite with an RAII Database class, 3NF tables, db-export,
example joins/aggregates and a conflicts view. This is the DBMS bonus.

ACCEPTANCE
- scripts/build.sh && scripts/run_demo.sh works offline on a clean machine and shows every item listed above.
- Binary round-trip and random-access tests pass; ledger tamper test fails verification as expected.
- UI shows all five tabs with real data.
- Whole repo: zero warnings, sanitizers clean, all C++ and Python tests pass, no placeholders.
```

---

## 4. Review checklist (use at the end of each phase before merging)

- [ ] Phase builds from a clean clone with `scripts/build.sh`.
- [ ] Test output in `HANDOFF_PHASE_N.md` is real, not typed by hand.
- [ ] Every concept the phase claims has a row in `docs/concept_matrix/phaseN.md` pointing at real code and a test.
- [ ] The phase demo command works exactly as written.
- [ ] No file from an earlier phase was changed except small additive edits that are listed in the handoff note.
- [ ] Next person can start from the handoff note alone.

## 5. If someone falls behind

Phases 3 and 4 depend on 1 and 2, so protect Phase 1 first. If time runs short, cut in this order: Phase 8 (DBMS), toy RSA, Hamiltonian demo, graph isomorphism, LLM extractor. Do not cut tests, the concept matrix or the sanitizer build.
