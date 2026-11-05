# COMPLEXITY.md — structures and algorithms

Phase 1 entries. All benchmarks numbers live in `docs/BENCHMARKS.md` (Phase 4).

## Data structures (verity::ds)

| Structure | Operation | Complexity | Notes |
|---|---|---|---|
| `LinkedList<T>` | push_front / push_back / pop_front | O(1) | tail pointer; singly linked |
| | pop_back | O(n) | needs predecessor |
| | removeFirstMatching | O(n) | |
| | copy / move | O(n) / O(1) | deep copy via clone of nodes |
| | iteration | O(n) | |
| `Stack<T>` / `Queue<T>` | push/pop/dequeue/peek | O(1) | composed on LinkedList |
| `HashTable<K,V>` | insert / get / contains / remove | O(1) avg, O(n) worst | separate chaining on LinkedList |
| | rehash | O(n + m) | prime sizing via `dm::nextPrime`, growth at load factor > 0.75 |
| | iteration | O(n + m) | |
| `Graph<N,E>` | addNode / addEdge | O(1) amortized | adjacency lists + own HashTable index |
| | neighbors / degrees | O(1) after lookup | |
| | removeEdge | O(deg(a)) | |
| | removeNode | O(V + E) | re-indexes |
| | transpose | O(V + E) | |
| | hasEdge | O(deg(a)) | scans out-list |

## Graph algorithms (verity::ds)

| Algorithm | Complexity | Notes |
|---|---|---|
| BFS k-hop (`bfsKHop`) | O(V + E) | stops expanding past depth k; own Queue |
| BFS shortest path | O(V + E) | parent map on own HashTable |
| DFS recursive / iterative | O(V + E) | pre-order; iterative uses own Stack, reverse-push |

## Discrete math (verity::dm)

| Function | Complexity | Notes |
|---|---|---|
| gcd / extendedGcd | O(log min(a,b)) | Euclid |
| lcm | O(log) | overflow-guarded (throws) |
| modPow | O(log e) | portable overflow-safe multiply |
| modInverse | O(log m) | throws when gcd(a,m) ≠ 1 |
| isPrime | O(√n) | trial division, 6k±1 |
| sieve(n) | O(n log log n) | bit flags |
| nextPrime | O(k·√n) | strictly greater |
| eulerTotient | O(√n) | factorization |
| FiniteSet insert/contains | O(log n) / O(n) | sorted vector (bitset analog documented) |
| FiniteSet algebra ops | O(n + m) | |
| powerSet | O(2ⁿ·n), n ≤ 20 | throws above the cap |
| factorial | O(n), exact n ≤ 20 | throws at 21! |
| nCr / nPr | O(k) mult/div | overflow-guarded |
| pascalNcr | O(n·k) additions | overflow-guarded |

## Engine

| Operation | Complexity | Notes |
|---|---|---|
| JSON parse + validation | O(bytes) | nlohmann parse + O(spans × slice) verification |
| GraphBuilder.build | O(D + N + C + EV + edges) | |
| VerityEngine.neighbors | O(V + E) | BFS k-hop |
| queryEvidence | O(entities + claims) | canonical name + alias match |

## Phase 3 additions

| Structure/Algorithm | Complexity | Notes |
|---|---|---|
| Formula parse | O(n) | shunting-yard |
| truthTable | O(2^v · n), v ≤ 12 | guarded throw beyond |
| forwardChain | O(rounds · rules) | fixpoint detection |
| resolution | O(clauses²), ≤ 32 clauses | guarded throw beyond |
| Dijkstra | O((V+E) log V) | handwritten MinHeap |
| Kruskal | O(E log E) | DSU + undirected projection |
| Prim | O(E log V) | heap-based |
| Tarjan SCC | O(V + E) | iterative |
| cut vertices/bridges | O(V + E) | recursive DFS (evidence-sized graphs) |
| Rabin-Karp | O(n + m) expected | verified matches only |
| bipartite/greedy coloring | O(V + E) / O(V·deg) | BFS 2-coloring / first-fit |
| A^k walks | O(k·V³) | dense matrix power, wrapping uint64 |

## Phase 4 additions

| Structure/Algorithm | Complexity | Notes |
|---|---|---|
| GraphSerializer save | O(records) | length-prefixed, atomic temp+rename |
| loadIndex | O(index) | trailer + index only |
| fetchRecord | O(index + record) | seekg straight to the record |
| isProbablePrime | O(12 · log³ n) | deterministic witness set over uint64 |
| crt | O(r · log m) | teaching-scale bounds, guarded |
| ledger verify | O(n) | recompute + stored-head comparison |
| isTree / isSpanningTree | O(V + E) | BFS reachability + edge-count law |
| GraphMatrix addEdge/hasEdge | O(1) | O(V²) memory — sparse graphs lose (see BENCHMARKS.md) |
| GraphMatrix build | O(V²) | measured: 1.9 s at 10k nodes vs 11.6 ms list |
