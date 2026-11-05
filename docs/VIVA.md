# VIVA.md — 10 questions per module + 5-minute demo script

## OOP (10)

1. **Why is Node's copy constructor protected and user-defined, not defaulted?**
   Public copy invites slicing; a PROTECTED copy lets derived `clone()` work.
   It is user-defined because a defaulted copy skips the live-object counter
   increment — clones would drift `liveObjects()` negative (caught by test).
2. **Rule of Five in NodeStore — what breaks without each member?**
   No copy ctor → shallow delete double-free; no copy assign → self-assignment
   corruption (guarded explicitly); no move → needless copies; no move assign
   → stale pointer leak on reassignment; no virtual ~Node → delete-through-
   base leaks.
3. **How does the Author diamond guarantee one Node subobject?**
   Person/SourceNode inherit `virtual Node`; the most-derived class (Author)
   initializes the virtual base directly. The test proves the Node* through
   both paths is the same address.
4. **Where are static vs dynamic binding demonstrably different?**
   `Node::type()` virtual → dynamic (base pointer calls the derived override);
   the shadowed `BindBase::tag` → static (base version through base pointer).
5. **What is the alias-union monoid and why AliasSet?**
   Entity objects alone lack a well-defined identity; the honest domain is
   AliasSet (set of alias strings) with union and the EMPTY set as identity —
   a genuine commutative monoid, exhaustively checked over a test domain.
6. **Why is `operator>>` on ClaimNode safe?**
   It reads a confidence value and routes through the VALIDATED setter, so
   stream input cannot smuggle an out-of-range confidence.
7. **How do the Evidence siblings avoid inheritance chains?**
   Supporting/Contradicting/Neutral all derive directly from abstract
   Evidence — kind() is data-like behavior, not a hierarchy.
8. **Composite vs plain tree — what does TextUnit buy?**
   Uniform wordCount()/text()/offsetRange() over Section/Paragraph/Sentence;
   offsets are computed once and slice-verified.
9. **Why is IExportable::toJson a std::string while Node::toJson returns
   nlohmann::json?** Two same-name virtuals with different return types
   cannot coexist; the interfaces stay off Node-derived classes.
10. **Where is the ONLY singleton?** Logger (Meyers, mutex-guarded sinks);
    everything else is dependency-injected.

## Discrete Mathematics (10)

1. **Inclusion–exclusion correctness?** |A∪B| = |A|+|B|−|A∩B|; verified
   against brute force on hundreds of random pairs.
2. **Warshall vs matrix power — difference?** Warshall computes reachability
   (boolean or); A^k counts walks (integer sum) — different semirings.
3. **Why does a cycle invalidate the Hasse diagram?** A strict order must be
   irreflexive/asymmetric/transitive; a cycle breaks asymmetry, so the
   "reduction" would encode an invalid order — withheld, witness emitted.
4. **When is different-CEO NOT a contradiction?** Disjoint validity intervals
   (has_ceo is functional but time-indexed); overlap is required (frozen
   rule) — month-vs-day overlap is also never flagged.
5. **Why is "Unknown" polarity never a clash?** Unknown is missing knowledge,
   not an assertion of the opposite; treating it as negation fabricates
   findings.
6. **Noisy-OR: what laws hold and what doesn't it claim?** Commutative monoid
   over [0,1] with identity 0 (associativity in fp tolerance); it aggregates
   confidence — never a truth probability.
7. **Why do Carmichael numbers expose Fermat?** a^(n−1) ≡ 1 for all coprime a
   even though n is composite (561 = 3·11·17) — pinned by test, fixed by
   Miller-Rabin's 12-witness set.
8. **CRT failure modes implemented?** Non-coprime moduli rejected; inconsistent
   residues detected; combined-modulus overflow guarded — each with a test.
9. **Linear extensions of the diamond poset?** Exactly 2 (b/c order free),
   verified against brute-force enumeration on random posets.
10. **Pigeonhole in this project?** It justifies why blocked candidate pairs
    force collisions in bounded universes — and the checker validates the
    premise before claiming the conclusion.

## Data Structures & Algorithms (10)

1. **HashTable bucket design?** One handwritten LinkedList per bucket;
   prime capacity via `dm::nextPrime` (strictly greater), load factor 0.75.
2. **Why prime capacities?** Better average chain distribution for the
   polynomial string hash modulo a prime.
3. **moved-from HashTable invariant?** Reinitialized to a prime-sized empty
   table — a moved-from zero-bucket table would divide by zero in bucketOf
   (found by test).
4. **AVL rebalancing during insert?** Rebalance EVERY ancestor on the
   recursion path (initial bug: only the root — caught by the height test).
5. **Iterative DFS == recursive DFS — how?** Push neighbors in REVERSE
   adjacency order and skip visited at pop time; equality asserted on random
   graphs.
6. **Kruskal vs Prim agreement?** Same undirected projection, same spanning
   FOREST semantics for disconnected graphs; equal total weights asserted.
7. **Tarjan SCC iterative — why?** Deep recursion would overflow the stack on
   long chains; frames emulate the call stack explicitly.
8. **Bridge vs parallel edges?** Skip the tree-parent edge ONCE; a second
   parallel edge acts as a back edge, so parallel pairs are never bridges.
9. **Rabin-Karp false positives?** Hash matches are ALWAYS verified
   character-wise; rolling hash is an accelerator, not the oracle.
10. **Binary serializer random access — how does fetchRecord avoid a full
    read?** It reads the 8-byte trailer, seeks to the index, then seeks to the
    one record's offset — the record section is never scanned.

## AI / Extraction (10)

1. **Mock extractor honesty?** It replays gold BY DESIGN and its P/R output
   is labeled non-evidence; rule-based extraction never reads data/gold.
2. **Span integrity guarantee?** Every span (Python + C++) must satisfy
   text[start:end] == quote in CODE POINTS; byte offsets fail the check.
3. **Why code points, not UTF-16 or bytes?** The schema is language-neutral;
   Python str indices and the browser's conversion agree explicitly (the
   "Café Meridian" fixture proves cp-count ≠ byte-count).
4. **Negation cue handling?** Cue window flips polarity to Negative; the
   60-char window's scope leak is PUBLISHED by test (honest limitation).
5. **LLM extractor safety?** Env-only config, strict JSON prompt, schema
   validation, one bounded retry, strict span discard, fake-client tests —
   no network in the offline pipeline.
6. **Strict span discard — no re-anchoring?** A claim whose quote does not
   slice exactly is dropped; never silently bound to the first match.
7. **P/R denominators?** TP/FP/FN with zero-division guards (None printed as
   "-"); greedy first-match on sorted keys, deterministic.
8. **Why is the mock P/R 1.0?** It replays gold — perfect BY CONSTRUCTION,
   which is exactly why it must never be quoted as quality evidence.
9. **PDF/DOCX offsets?** pypdf/python-docx text is normalized with the same
   contract and paragraph offsets are verified by slicing.
10. **Schema validation is real?** jsonschema (draft 2020-12) in Python PLUS
    an independent structural+semantic validator in C++ — parsing alone is
    never called validation.

## Engine / product (10)

1. **ConflictDetector extension rule?** New ConflictRule subclasses plug in
   with zero detector changes (Strategy) — proven by a test-local rule.
2. **What does the rules.txt DSL guarantee?** A frozen grammar; violations
   throw RuleError — rule strings are never eval'd or executed.
3. **Observer pattern location?** ConflictDetector notifies ConflictListener
   subscribers (CLI printer + JSON collector in production).
4. **Why re-load per command?** Cross-process honesty for phase 1: the session
   file records the extraction path; deterministic reloads, no stale memory.
5. **Binary format safety?** Magic + version + length-prefixed fields +
   bounds-checked offsets + atomic temp/rename; no raw C++ object dumps.
6. **Ledger tamper detection flow?** Verify = recompute chain, compare STORED
   head; a tampered COPY fails while the pristine file verifies (demo step 9).
7. **Where does the UI get data?** Only the exported report payload (with the
   file-import fallback); the UI never pretends to extract documents.
8. **Word enforcement?** Findings carry the exact phrases; a test walks every
   exported conflict and checks the wording.
9. **Fixture cross-check in the DM Lab?** Browser computations are compared
   against precomputed engine fixture values embedded in the report.
10. **What is withheld for cyclic temporal data?** Hasse diagram and
    topological order — an invalid order is never rendered as verified.

## 5-minute demo script

1. (0:00) `scripts/run_demo.sh` — narrate the printed steps: extraction
   counts, ingest with entity resolution + cycle witness.
2. (1:00) `verity conflicts` — show the planted date/CEO/alibi/negation/
   numeric findings with quotes and benign explanations.
3. (2:00) `verity circular` + `verity timeline` — the citation cycle witness
   and the temporal contradiction with Hasse withheld.
4. (2:40) `verity save/load --fetch C010` — binary persistence with indexed
   random access; `verity verify-ledger` then the tampered copy failing.
5. (3:20) `verity export` — open `ui/index.html`, walk Graph → Conflicts →
   Timeline → Provenance → DM Lab (live computation + fixture matches).
6. (4:20) `verity dm logic` + `verity dm graphs` — the math layer answering.
7. (4:40) Point at `docs/CONCEPT_MATRIX.md`, `out/validation/*.log` — every
   claimed concept has a file:line, a test and a log.
