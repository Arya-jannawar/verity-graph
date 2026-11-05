# DM UNIT 2 — Counting, recurrences, induction (Phase 1: counting basics)

> Phase 2 adds permutations/combinations algorithms and linear extensions;
> Phase 4 adds recurrences and induction proofs.

## 1. Counting basics (implemented in Phase 1)

### Definitions
- `n!` with `0! = 1`; permutations `P(n,k) = n!/(n−k)!`; combinations
  `C(n,k) = n!/(k!(n−k)!)` (symmetry `C(n,k) = C(n,n−k)`).
- **Pascal's rule:** `C(n,k) = C(n−1,k−1) + C(n−1,k)`.
- **Binomial theorem:** `(a+b)^n = Σ_k C(n,k) a^k b^(n−k)`.

### Theorem (Pascal identity, by double counting)
Fix element x in an n-set. Subsets of size k either contain x — choose the
remaining k−1 from n−1 elements: `C(n−1,k−1)` — or avoid x — `C(n−1,k)`.
The two cases are disjoint and exhaustive, giving Pascal's rule; iterating
the rule builds the triangle and yields an addition-only DP that computes
`C(n,k)` in `O(n·k)` without division.

### Use in Verity
- Overflow-safe counting of candidate pair volumes (e.g. bounded power sets
  `2^|A|` guard at n = 20) so combinatorial demos never freeze.
- HashTable capacity arithmetic uses prime rounding rather than counting, but
  the *load-factor thresholds* are derived from counting arguments.
- `binomialTheoremCheck` demonstrates coefficient identities used when
  aggregating independent evidence counts (Phase 3 noisy-OR tests reuse the
  "wrap-in-a-documented-ring" pattern).

### Worked example
`C(52,5)` (poker hands) `= 2,598,960` — computed by the multiplicative method
in 5 division steps. `21!` exceeds 2^64, so `factorial(21)` throws instead of
wrapping (run `verity dm counting`).

### Tests
`tests/dm/test_counting.cpp`: "factorial: exact 0..20 and overflow throw at 21",
"nCr: known values, symmetry, errors", "pascalNcr agrees with nCr over the
full grid n<=25", "binomial theorem identity (a+b)^n over uint64 ring".

## 2. Permutations, combinations, linear extensions (Phase 2)

### Definitions
- Lexicographic next-permutation transforms a sequence into its successor.
- k-combinations: recursive inclusion/exclusion tree or iterative index
  scheme — both enumerate C(n,k) subsets.
- A **linear extension** of a poset is a total order respecting all
  relations; the number of extensions is counted by backtracking.
- **Pigeonhole principle:** n+1 items in n boxes force a box with ≥2 items.

### Theorem (linear extensions of a chain)
A total chain a₁<a₂<…<aₙ has EXACTLY ONE linear extension. Verified on the
chain test; the diamond a<b, a<c, b<d, c<d has exactly two.

### Use in Verity
- The topological order produced by TimelineBuilder IS a linear extension of
  the event order; `countLinearExtensions` demonstrates how many exist
  (the DM Lab displays this for editable posets).
- Enumeration guards (n ≤ 8 permutations, n ≤ 9 extensions, power set ≤ 20)
  keep every interactive demo bounded instead of freezing.

### Tests
tests/dm/test_combinatorics.cpp (std::next_permutation oracle, recursive vs
iterative agreement, brute-force linear-extension oracle on random posets).
