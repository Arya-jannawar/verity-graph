# DM UNIT 5 — Graph theory (Phase 3)

## Definitions
Walk (vertices may repeat), trail (edges distinct), path (vertices distinct),
cycle. Connected(undirected), Euler circuit/path (0 or 2 odd-degree vertices,
connected core), Hamiltonian path (visits every vertex once). Bipartite ⇔ no
odd cycle (Kőnig). Handshake lemma: Σdeg = 2|E| (digraph: Σin = Σout = |A|).
A^k counts length-k WALKS between vertex pairs.

## Theorem (handshake)
Every undirected graph satisfies Σdeg(v) = 2|E|: each edge contributes 1 to
the degree of each endpoint — a self-loop contributes 2 to one vertex.
*Proof:* direct counting argument over the incidence relation.

## Use in Verity
- `dm::GraphAnalysis` validates the engine's projections (handshake check,
  bipartite coloring of the evidence graph, greedy coloring, A^k walk counts
  for reachability demos).
- Euler/connectedness checks explain WHY the provenance/origin structures can
  or cannot be walked as expected.
- Hamiltonian brute force (guarded ≤ 10 vertices) demonstrates exponential
  search bounds.

## Worked example
C4 cycle A-B-C-D-A: all degrees 2 → Σdeg = 8 = 2·4 ✓; bipartite ✓ (even
cycle); Euler circuit ✓; A²[C→A walks] = 2 (A-B-C and A-D-C); A⁴[A→A] = 8
(eigenvalue check (16+0+16+0)/4). Run `verity dm graphs`.

## Tests
tests/dm/test_graphanalysis.cpp (handshake incl. self-loop convention,
bipartite odd/even cycles, Euler transitions, guarded Hamiltonian, A^k
eigenvalue-checked counts).
