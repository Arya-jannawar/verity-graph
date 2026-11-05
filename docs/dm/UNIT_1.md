# DM UNIT 1 — Set theory, relations, functions (Phase 1: sets section)

> Phase 2 adds the relations and functions sections; Phase 3 adds the logic
> section. Definitions/theorem/Verity-use/worked-example/test-names format.

## 1. Sets (implemented in Phase 1)

### Definitions
A **finite set** is an unordered collection of distinct elements. For sets A, B:
union `A∪B = {x : x∈A ∨ x∈B}`, intersection `A∩B = {x : x∈A ∧ x∈B}`,
difference `A\B = {x : x∈A ∧ x∉B}`, symmetric difference `A⊕B = (A\B)∪(B\A)`,
Cartesian product `A×B = {(a,b) : a∈A, b∈B}`, power set `P(A)` = set of all
subsets (`|P(A)| = 2^|A|`).

### Theorem (inclusion–exclusion)
For finite sets: `|A∪B| = |A| + |B| − |A∩B|`.
*Proof sketch:* every element of `A∩B` is counted exactly twice by `|A|+|B|`
and must be counted once; subtract one copy. Generalized IE follows by
alternating sums over all intersections (used for ≥3 sets in Phase 2 partitions).

### Use in Verity
- Alias sets of an entity (union of mention names) — alias union semantics
  (Phase 2 monoid test builds on the empty-identity rule frozen in CONTRACT §10).
- Entity-resolution candidate blocking uses set intersection of candidate
  buckets (Phase 2).
- The **empty set is the identity of union** — the analog of the "unknown
  merge" identity for Entity merge in Phase 2.

### Worked example
`A = {1,2,3,4}`, `B = {3,4,5}`: `|A|=4, |B|=3, |A∩B|=2` → `|A∪B| = 4+3−2 = 5`.
IE vs brute force agree: `{1,2,3,4,5}` has 5 elements. Run: `verity dm sets`.

### Tests
`tests/dm/test_sets.cpp`: "FiniteSet: algebra laws over fixed sets",
"FiniteSet: randomized oracle vs std::set", "set-operation oracle on random
pairs + IE identity", "power set sizes and content for n<=12, guard above 20".
