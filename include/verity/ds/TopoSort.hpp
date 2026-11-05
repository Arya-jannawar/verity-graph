#pragma once
// VERITY GRAPH — ds/TopoSort: topological ordering and cycle detection
// (Phase 2). TimelineBuilder orders events with Kahn's algorithm and reports
// happened-before cycles with witness paths; undirected cycle detection backs
// the MST/bridge sanity checks (Phase 3).
//
// Complexity summary (V vertices, E edges):
//   kahnTopoSort        O(V + E)   empty result on cycle
//   dfsTopoSort         O(V + E)   empty result on cycle
//   directedCycle       O(V + E)   colors; returns witness path or {}
//   undirectedHasCycle  O(V·α(V))  DSU over edges

#include <algorithm>
#include <functional>
#include <vector>

#include "verity/ds/DisjointSet.hpp"
#include "verity/ds/Graph.hpp"
#include "verity/ds/HashTable.hpp"
#include "verity/ds/Queue.hpp"
#include "verity/ds/Stack.hpp"

namespace verity::ds {

/// Kahn's algorithm. Returns nodes in topological order, or EMPTY when the
/// graph has a directed cycle (not all nodes become removable).
template <class N, class E>
[[nodiscard]] std::vector<N> kahnTopoSort(const Graph<N, E>& graph) {
    const auto& nodes = graph.nodes();
    HashTable<N, std::size_t> indegree;
    for (const N& node : nodes) indegree.insert(node, 0);
    for (const N& node : nodes) {
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            (*indegree.get(to))++;
        }
    }
    Queue<N> ready;
    for (const N& node : nodes) {
        if (*indegree.get(node) == 0) ready.enqueue(node);
    }
    std::vector<N> order;
    order.reserve(nodes.size());
    while (!ready.empty()) {
        const N node = ready.dequeue();
        order.push_back(node);
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            (*indegree.get(to))--;
            if (*indegree.get(to) == 0) ready.enqueue(to);
        }
    }
    if (order.size() != nodes.size()) return {};
    return order;
}

/// DFS-based topological sort (post-order reversed). Empty on cycle.
template <class N, class E>
[[nodiscard]] std::vector<N> dfsTopoSort(const Graph<N, E>& graph) {
    enum class Mark { Unvisited, InProgress, Done };
    HashTable<N, Mark> marks;
    std::vector<N> postorder;
    bool cyclic = false;

    std::function<void(const N&)> visit = [&](const N& node) {
        *marks.get(node) = Mark::InProgress;
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            const Mark m = *marks.get(to);
            if (m == Mark::Unvisited) {
                visit(to);
            } else if (m == Mark::InProgress) {
                cyclic = true;
            }
        }
        *marks.get(node) = Mark::Done;
        postorder.push_back(node);
    };

    for (const N& node : graph.nodes()) marks.insert(node, Mark::Unvisited);
    for (const N& node : graph.nodes()) {
        if (*marks.get(node) == Mark::Unvisited) visit(node);
        if (cyclic) return {};
    }
    if (cyclic) return {};
    return std::vector<N>(postorder.rbegin(), postorder.rend());
}

/// Directed cycle detection (white/gray/black). Returns a witness path
/// (start == end) when a cycle exists, empty vector otherwise.
template <class N, class E>
[[nodiscard]] std::vector<N> directedCycle(const Graph<N, E>& graph) {
    enum class Mark { Unvisited, InProgress, Done };
    HashTable<N, Mark> marks;
    HashTable<N, N> parent;
    std::vector<N> witness;

    std::function<bool(const N&)> visit = [&](const N& node) -> bool {
        *marks.get(node) = Mark::InProgress;
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            const Mark m = *marks.get(to);
            if (m == Mark::Unvisited) {
                parent.insert(to, node);
                if (visit(to)) return true;
            } else if (m == Mark::InProgress) {
                // reconstruct the witness: to ... node, to
                witness.clear();
                witness.push_back(to);
                N cur = node;
                while (true) {
                    witness.push_back(cur);
                    if (cur == to) break;
                    const N* p = parent.get(cur);
                    if (p == nullptr) break;
                    cur = *p;
                }
                std::reverse(witness.begin(), witness.end());
                return true;
            }
        }
        *marks.get(node) = Mark::Done;
        return false;
    };

    for (const N& node : graph.nodes()) marks.insert(node, Mark::Unvisited);
    for (const N& node : graph.nodes()) {
        if (*marks.get(node) == Mark::Unvisited) {
            if (visit(node)) return witness;
        }
    }
    return {};
}

/// Undirected cycle detection via DSU: adding an edge whose endpoints are
/// already connected closes a cycle. Uses first-seen indices.
template <class N, class E>
[[nodiscard]] bool undirectedHasCycle(const Graph<N, E>& graph) {
    const auto& nodes = graph.nodes();
    DisjointSet dsu(nodes.size());
    HashTable<N, std::size_t> index;
    for (std::size_t i = 0; i < nodes.size(); ++i) index.insert(nodes[i], i);
    // Each undirected pair appears twice in adjacency; visit only i < j pairs.
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (const auto& [to, w] : graph.neighbors(nodes[i])) {
            (void)w;
            const std::size_t j = *index.get(to);
            if (j < i) continue;   // handle each pair once
            if (!dsu.unionSets(i, j)) return true;
        }
    }
    return false;
}

} // namespace verity::ds
