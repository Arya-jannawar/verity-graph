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

## 2. Relations (implemented in Phase 2)

### Definitions
A binary relation R over a finite universe U is a subset of U×U. Properties:
reflexive (∀x xRx), irreflexive, symmetric, antisymmetric (aRb ∧ bRa ⟹ a=b),
asymmetric, transitive. Equivalence = reflexive+symmetric+transitive;
(partial) order = reflexive+antisymmetric+transitive; strict order =
irreflexive+asymmetric+transitive. Closures add the minimum pairs to obtain a
property; **Warshall's algorithm** computes the transitive closure in O(n³).
Equivalence classes form a **partition** (disjoint + covering). For a poset:
Hasse diagram = transitive reduction; minimal/maximal elements; lower/upper
bounds; lub/glb; lattice when every pair has both; comparable/incomparable.

### Theorem (Warshall correctness idea)
After processing pivot k, entry m[i][j] records "there is a path i→j using
only intermediate vertices < k". Induction on k: adding paths through k
merges paths i→k and k→j already known to use intermediates < k.

### Use in Verity
- EntityResolver: matched entity pairs populate a `Relation`; the **equivalence
  closure** yields canonical classes; DisjointSet computes the same partition
  independently and the two must agree.
- TimelineBuilder: `happened_before` (+ precise disjoint dates) build a strict
  order; a cycle breaks asymmetry → the Hasse diagram and topological order
  are WITHHELD and a "Potential temporal contradiction" finding with a witness
  path is emitted instead.

### Worked example
Divides-relation on {1,2,3,4,6,12}: lub(2,3)=6, glb(4,6)=2, a lattice.
Hasse cover edges drop 1<3, 1<4, 1<6, 1<12, 2<4(implied? no — 2<4 kept, 2<12
implied by 2<6<12), etc. Run: `verity dm relations`.

### Tests
tests/dm/test_relations.cpp (10 cases incl. randomized Warshall oracle,
DisjointSet agreement, Hasse exactness, partition checks).

## 3. Functions (implemented in Phase 2)

### Definitions
A finite function maps every domain element to at most one codomain element.
total / injective / surjective / bijective; composition g∘f; inverse exists
iff bijective.

### Use in Verity
The mention→canonical-entity mapping is verified **total and onto** the
canonical entity set (`isTotalSurjective`) — every mention resolves and every
canonical entity is reachable. Inverse corresponds to expanding a canonical
entity back to its mentions.

### Tests
tests/dm/test_functions.cpp (classification, composition/inverse, failure
paths, the mention→canonical pattern).
