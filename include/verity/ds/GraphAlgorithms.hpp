#pragma once
// VERITY GRAPH — ds/GraphAlgorithms: Dijkstra, Kruskal, Prim, Tarjan SCC,
// articulation points and bridges (Phase 3).
//
// SEMANTICS (frozen, honest):
//   * Dijkstra runs with edge weight = 1 - confidence. It MINIMIZES this
//     defined additive cost — the result is NOT a generic "truth score".
//   * Kruskal/Prim operate on the UNDIRECTED PROJECTION of the graph
//     (each undirected pair contributes its FIRST parallel edge); disconnected
//     graphs yield a minimum spanning FOREST. Self-loops never enter an MST.
//   * Tarjan SCC runs on the DIRECTED graph as given.
//   * Articulation points and bridges run on the UNDIRECTED projection with
//     the standard skip-tree-parent-once rule (parallel edges correctly stop
//     bridges). Implemented recursively — intended for evidence-sized graphs.
//
// Complexity summary (V vertices, E edges):
//   dijkstra                     O((V + E) log V)  with the handwritten MinHeap
//   kruskalMst                   O(E log E)
//   primMst                      O(E log V)
//   tarjanScc                    O(V + E)          iterative
//   cutVerticesAndBridges        O(V + E)          recursive DFS

#include <algorithm>
#include <limits>
#include <functional>
#include <set>
#include <tuple>
#include <vector>

#include "verity/ds/DisjointSet.hpp"
#include "verity/ds/Graph.hpp"
#include "verity/ds/HashTable.hpp"
#include "verity/ds/Heap.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

// --- Dijkstra ---------------------------------------------------------------

/// Shortest additive-cost paths from `start`. Returns {node, distance} in
/// discovery order for reachable nodes (distance 0 for the start).
template <class N, class E, class CostOf>
[[nodiscard]] std::vector<std::pair<N, double>> dijkstra(const Graph<N, E>& graph,
                                                         const N& start, CostOf costOf) {
    MinHeap<std::pair<double, N>> heap;
    HashTable<N, double> dist;
    std::vector<std::pair<N, double>> out;
    dist.insert(start, 0.0);
    heap.push({0.0, start});
    while (!heap.empty()) {
        const auto [d, node] = heap.pop();
        if (d > *dist.get(node) + 1e-12) continue;   // stale heap entry
        out.emplace_back(node, d);
        for (const auto& [to, weight] : graph.neighbors(node)) {
            const double candidate = d + costOf(weight);
            if (!dist.contains(to)) {
                dist.insert(to, candidate);
                heap.push({candidate, to});
            } else if (candidate < *dist.get(to) - 1e-12) {
                *dist.get(to) = candidate;
                heap.push({candidate, to});
            }
        }
    }
    return out;
}

/// Distance from start to goal; +infinity when unreachable.
template <class N, class E, class CostOf>
[[nodiscard]] double dijkstraDistance(const Graph<N, E>& graph, const N& start, const N& goal,
                                      CostOf costOf) {
    for (const auto& [node, d] : dijkstra(graph, start, costOf)) {
        if (node == goal) return d;
    }
    return (std::numeric_limits<double>::infinity)();
}

// --- MST (undirected projection) ----------------------------------------------

/// Minimum spanning forest of the UNDIRECTED projection (Kruskal + DSU).
/// Returns {a, b, weight} triples.
template <class N, class E, class WeightOf>
[[nodiscard]] std::vector<std::tuple<N, N, double>> kruskalMst(const Graph<N, E>& graph,
                                                               WeightOf weightOf) {
    std::set<std::pair<N, N>> seen;
    std::vector<std::tuple<double, N, N>> edges;
    for (const N& node : graph.nodes()) {
        for (const auto& [to, weight] : graph.neighbors(node)) {
            const std::pair<N, N> key = node < to ? std::make_pair(node, to)
                                                  : std::make_pair(to, node);
            if (key.first == key.second) continue;          // self-loops excluded
            if (!seen.insert(key).second) continue;         // projection: pair once
            edges.emplace_back(weightOf(weight), key.first, key.second);
        }
    }
    std::sort(edges.begin(), edges.end());
    DisjointSet dsu(graph.nodes().size());
    HashTable<N, std::size_t> index;
    for (std::size_t i = 0; i < graph.nodes().size(); ++i) index.insert(graph.nodes()[i], i);
    std::vector<std::tuple<N, N, double>> forest;
    for (const auto& [w, a, b] : edges) {
        if (dsu.unionSets(*index.get(a), *index.get(b))) {
            forest.emplace_back(a, b, w);
        }
    }
    return forest;
}

/// Prim's MST over the same undirected projection, grown per component so the
/// result agrees with Kruskal's forest policy.
template <class N, class E, class WeightOf>
[[nodiscard]] std::vector<std::tuple<N, N, double>> primMst(const Graph<N, E>& graph,
                                                            WeightOf weightOf) {
    HashTable<N, bool> inTree;
    std::vector<std::tuple<N, N, double>> forest;
    using HeapItem = std::tuple<double, N, N>;   // (weight, from, to)
    for (const N& start : graph.nodes()) {
        if (inTree.contains(start)) continue;
        inTree.insert(start, true);
        MinHeap<HeapItem> heap;   // min-heap by tuple order: smallest weight first
        for (const auto& [to, weight] : graph.neighbors(start)) {
            if (to != start) heap.push({weightOf(weight), start, to});
        }
        while (!heap.empty()) {
            const auto [w, from, to] = heap.pop();
            if (inTree.contains(to)) continue;
            inTree.insert(to, true);
            forest.emplace_back(from, to, w);
            for (const auto& [next, weight] : graph.neighbors(to)) {
                if (!inTree.contains(next)) {
                    heap.push({weightOf(weight), to, next});
                }
            }
        }
    }
    return forest;
}

// --- Tarjan SCC (directed, iterative) ------------------------------------------

/// Tarjan strongly connected components. Returns components in reverse
/// topological order (each component sorted).
template <class N, class E>
[[nodiscard]] std::vector<std::vector<N>> tarjanScc(const Graph<N, E>& graph) {
    struct Frame {
        N node;
        std::size_t childIndex;
    };
    HashTable<N, std::size_t> index, low;
    HashTable<N, bool> onStack;
    std::vector<N> stack;
    std::vector<Frame> frames;
    std::vector<std::vector<N>> components;
    std::size_t counter = 0;

    for (const N& root : graph.nodes()) {
        if (index.contains(root)) continue;
        frames.push_back({root, 0});
        index.insert(root, counter);
        low.insert(root, counter);
        counter++;
        stack.push_back(root);
        onStack.insert(root, true);

        while (!frames.empty()) {
            Frame& frame = frames.back();
            const auto& neighbors = graph.neighbors(frame.node);
            if (frame.childIndex < neighbors.size()) {
                const N next = neighbors[frame.childIndex++].first;
                if (!index.contains(next)) {
                    index.insert(next, counter);
                    low.insert(next, counter);
                    counter++;
                    stack.push_back(next);
                    onStack.insert(next, true);
                    frames.push_back({next, 0});
                } else if (onStack.contains(next)) {
                    *low.get(frame.node) =
                        std::min(*low.get(frame.node), *index.get(next));
                }
            } else {
                const N node = frame.node;
                frames.pop_back();
                if (!frames.empty()) {
                    *low.get(frames.back().node) =
                        std::min(*low.get(frames.back().node), *low.get(node));
                }
                if (*low.get(node) == *index.get(node)) {
                    std::vector<N> component;
                    while (true) {
                        const N member = stack.back();
                        stack.pop_back();
                        onStack.insert(member, false);
                        component.push_back(member);
                        if (member == node) break;
                    }
                    std::sort(component.begin(), component.end());
                    components.push_back(std::move(component));
                }
            }
        }
    }
    return components;
}

// --- Cut vertices and bridges (undirected projection, recursive) ---------------

template <class N>
struct CutResult {
    std::vector<N> articulationPoints;
    std::vector<std::pair<N, N>> bridges;
};

template <class N, class E>
[[nodiscard]] CutResult<N> cutVerticesAndBridges(const Graph<N, E>& graph) {
    HashTable<N, std::size_t> disc, low;
    HashTable<N, bool> visited;
    HashTable<N, bool> articulation;
    std::vector<std::pair<N, N>> bridges;
    std::size_t counter = 0;

    std::function<void(const N&, const N&, bool)> dfs =
        [&](const N& u, const N& parent, bool isRoot) {
            visited.insert(u, true);
            disc.insert(u, counter);
            low.insert(u, counter);
            ++counter;
            int children = 0;
            std::set<N> parentSkipped;   // skip tree parent edge exactly once
            for (const auto& [v, w] : graph.neighbors(u)) {
                (void)w;
                if (v == u) continue;   // self-loop
                if (!visited.contains(v)) {
                    ++children;
                    dfs(v, u, false);
                    *low.get(u) = std::min(*low.get(u), *low.get(v));
                    if (!isRoot && *low.get(v) >= *disc.get(u)) {
                        articulation.insert(u, true);
                    }
                    if (*low.get(v) > *disc.get(u)) {
                        bridges.emplace_back(u, v);
                    }
                } else if (v != parent || parentSkipped.count(v) != 0u) {
                    // back edge; a SECOND parallel edge to the parent also
                    // counts (so parallel pairs are never bridges)
                    *low.get(u) = std::min(*low.get(u), *disc.get(v));
                } else {
                    parentSkipped.insert(v);
                }
            }
            if (isRoot && children >= 2) articulation.insert(u, true);
        };

    for (const N& root : graph.nodes()) {
        if (!visited.contains(root)) dfs(root, root, true);
    }

    CutResult<N> out;
    for (const N& n : graph.nodes()) {
        if (articulation.contains(n) && *articulation.get(n)) {
            out.articulationPoints.push_back(n);
        }
    }
    std::sort(bridges.begin(), bridges.end());
    bridges.erase(std::unique(bridges.begin(), bridges.end()), bridges.end());
    out.bridges = std::move(bridges);
    return out;
}

} // namespace verity::ds
