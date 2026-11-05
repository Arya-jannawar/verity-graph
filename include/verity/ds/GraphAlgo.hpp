#pragma once
// VERITY GRAPH — GraphAlgo: BFS / DFS over ds::Graph using the handwritten
// Queue, Stack and HashTable (handwritten structures in real engine paths).
//
// Complexity summary (V vertices, E edges, reachable set R):
//   bfsKHop          O(V + E)     (stops expanding past depth k)
//   bfsShortestPath  O(V + E)
//   dfsRecursive     O(V + E)     (pre-order)
//   dfsIterative     O(V + E)     (pre-order, equal to dfsRecursive)
//
// Determinism: traversal order follows adjacency insertion order.

#include <utility>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/HashTable.hpp"
#include "verity/ds/Queue.hpp"
#include "verity/ds/Stack.hpp"

namespace verity::ds {

/// BFS restricted to k hops: returns {node, distance} pairs for all reachable
/// nodes at distance 0..k (start included at distance 0), in discovery order.
template <class N, class E>
[[nodiscard]] std::vector<std::pair<N, int>> bfsKHop(const Graph<N, E>& graph,
                                                     const N& start, int maxHops) {
    std::vector<std::pair<N, int>> result;
    if (!graph.hasNode(start) || maxHops < 0) return result;

    HashTable<N, bool> visited;
    Queue<std::pair<N, int>> frontier;
    visited.insert(start, true);
    frontier.enqueue({start, 0});
    while (!frontier.empty()) {
        const auto [node, depth] = frontier.dequeue();
        result.emplace_back(node, depth);
        if (depth >= maxHops) continue;
        for (const auto& [neighbor, weight] : graph.neighbors(node)) {
            (void)weight;
            if (!visited.contains(neighbor)) {
                visited.insert(neighbor, true);
                frontier.enqueue({neighbor, depth + 1});
            }
        }
    }
    return result;
}

/// Unweighted shortest path start->goal (BFS), endpoints included; empty when
/// unreachable or when either endpoint is missing.
template <class N, class E>
[[nodiscard]] std::vector<N> bfsShortestPath(const Graph<N, E>& graph,
                                             const N& start, const N& goal) {
    if (!graph.hasNode(start) || !graph.hasNode(goal)) return {};
    if (start == goal) return {start};

    HashTable<N, N> parent;
    HashTable<N, bool> visited;
    Queue<N> frontier;
    visited.insert(start, true);
    frontier.enqueue(start);
    while (!frontier.empty()) {
        const N node = frontier.dequeue();
        for (const auto& [neighbor, weight] : graph.neighbors(node)) {
            (void)weight;
            if (visited.contains(neighbor)) continue;
            visited.insert(neighbor, true);
            parent.insert(neighbor, node);
            if (neighbor == goal) {
                // reconstruct
                std::vector<N> path{goal};
                N cur = goal;
                while (parent.contains(cur)) {
                    cur = *parent.get(cur);
                    path.push_back(cur);
                    if (cur == start) break;
                }
                std::vector<N> ordered(path.rbegin(), path.rend());
                return ordered;
            }
            frontier.enqueue(neighbor);
        }
    }
    return {};
}

namespace detail {

template <class N, class E>
void dfsRecursiveVisit(const Graph<N, E>& graph, const N& node,
                       HashTable<N, bool>& visited, std::vector<N>& order) {
    visited.insert(node, true);
    order.push_back(node);
    for (const auto& [neighbor, weight] : graph.neighbors(node)) {
        (void)weight;
        if (!visited.contains(neighbor)) {
            dfsRecursiveVisit(graph, neighbor, visited, order);
        }
    }
}

} // namespace detail

/// Depth-first pre-order (recursive), neighbors in adjacency insertion order.
template <class N, class E>
[[nodiscard]] std::vector<N> dfsRecursive(const Graph<N, E>& graph, const N& start) {
    std::vector<N> order;
    if (!graph.hasNode(start)) return order;
    HashTable<N, bool> visited;
    detail::dfsRecursiveVisit(graph, start, visited, order);
    return order;
}

/// Depth-first pre-order (iterative, handwritten Stack). Produces exactly the
/// dfsRecursive pre-order: neighbors are pushed in reverse insertion order and
/// stale (already visited) stack entries are skipped at pop time.
template <class N, class E>
[[nodiscard]] std::vector<N> dfsIterative(const Graph<N, E>& graph, const N& start) {
    std::vector<N> order;
    if (!graph.hasNode(start)) return order;
    HashTable<N, bool> visited;
    Stack<N> pending;
    pending.push(start);
    while (!pending.empty()) {
        const N node = pending.pop();
        if (visited.contains(node)) continue;
        visited.insert(node, true);
        order.push_back(node);
        const auto& out = graph.neighbors(node);
        for (std::size_t i = out.size(); i-- > 0;) {
            const N& neighbor = out[i].first;
            if (!visited.contains(neighbor)) {
                pending.push(neighbor);
            }
        }
    }
    return order;
}

} // namespace verity::ds
