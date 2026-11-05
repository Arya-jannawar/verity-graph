# DM UNIT 6 — Trees (Phase 4)

## Definitions
Tree = connected + |E| = |V| − 1 (acyclic follows). Full binary tree: every
internal node has exactly 2 children. Spanning tree of G: subgraph that is a
tree covering all vertices. Expression tree: operators internal, operands
leaves; pre/in/post-order walks.

## Theorem (full binary leaf law)
A full binary tree with i internal nodes has exactly i + 1 leaves.
*Proof sketch (induction on i):* i = 0 → single leaf node ✓. Adding one
internal node attaches two new leaves and converts one leaf to internal:
leaves −1 +2 = +1, internal +1 → invariant preserved.

## Use in Verity
- `ds::isTree` verifies the origin/document projections; `isSpanningTree`
  is the brute-force oracle for the Kruskal/Prim backbone cross-check.
- `ds::ExpressionTree` powers the DM Lab expression demo (postfix → tree →
  infix/prefix/postfix → evaluate) and demonstrates the leaf law live.
- Document tree traversals (Section/Paragraph/Sentence composite) reuse the
  same pre/post/level-order patterns as ds::NaryTree.

## Worked example
Postfix "2 3 + 4 *" → tree ((2+3)*4): infix "((2 + 3) * 4)", prefix
"* + 2 3 4", evaluates 20; i = 2 internal, leaves = 3 = i + 1 ✓.
Kruskal on the 5-node weighted graph yields 4 edges that pass isSpanningTree.

## Tests
tests/io/test_phase4.cpp ("tree law…", "arithmetic expression tree…");
tests/ds/test_ds2.cpp (NaryTree traversals).
