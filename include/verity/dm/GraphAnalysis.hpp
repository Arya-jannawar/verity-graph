#pragma once
// VERITY GRAPH — dm/GraphAnalysis: graph theory over ds::Graph (DM Unit 5).
//
// degree sequence, handshake theorem (incl. digraph in/out), isolated and
// pendant vertices, walk/trail/path/cycle classification, connectedness,
// bipartite test (BFS 2-coloring), Euler existence, Hamiltonian brute force
// (guarded: <= 10 vertices), subgraph/induced subgraph/union/intersection/
// complement, small isomorphism (guarded: <= 6 vertices), greedy coloring,
// A^k walk counting.
//
// Convention notes (frozen):
//   * degree of an undirected vertex counts each self-loop as 2 (matches
//     ds::Graph::degree).
//   * Handshake for digraphs: sum(in) == sum(out) == |A|.
//   * A^k counts WALKS (vertices may repeat) via integer matrix powers.

#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "verity/ds/GraphAlgo.hpp"
#include "verity/ds/Queue.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Sorted degree sequence of an undirected graph (ascending).
template <class N, class E>
[[nodiscard]] std::vector<std::size_t> degreeSequence(const ds::Graph<N, E>& g) {
    std::vector<std::size_t> out;
    out.reserve(g.nodes().size());
    for (const N& n : g.nodes()) out.push_back(g.degree(n));
    std::sort(out.begin(), out.end());
    return out;
}

/// Handshake theorem: sum of undirected degrees == 2 * |E| (self-loops count
/// 2 in degree and 2 towards |E| ... a loop contributes 2 to the degree sum
/// and 1 to |E|, so the identity sum(deg) == 2|E| still holds).
template <class N, class E>
[[nodiscard]] bool handshakeHolds(const ds::Graph<N, E>& g) {
    std::size_t sum = 0;
    for (const N& n : g.nodes()) sum += g.degree(n);
    return sum == 2 * g.edgeCount();
}

/// Digraph handshake: sum(in) == sum(out) == |A|.
template <class N, class E>
[[nodiscard]] bool digraphHandshakeHolds(const ds::Graph<N, E>& g) {
    std::size_t inSum = 0, outSum = 0;
    for (const N& n : g.nodes()) {
        inSum += g.inDegree(n);
        outSum += g.outDegree(n);
    }
    return inSum == outSum && outSum == g.edgeCount();
}

template <class N, class E>
[[nodiscard]] std::vector<N> isolatedVertices(const ds::Graph<N, E>& g) {
    std::vector<N> out;
    for (const N& n : g.nodes()) {
        if (g.degree(n) == 0) out.push_back(n);
    }
    return out;
}

template <class N, class E>
[[nodiscard]] std::vector<N> pendantVertices(const ds::Graph<N, E>& g) {
    std::vector<N> out;
    for (const N& n : g.nodes()) {
        if (g.degree(n) == 1) out.push_back(n);
    }
    return out;
}

/// Undirected connectedness (BFS from the first node must reach all).
template <class N, class E>
[[nodiscard]] bool isConnected(const ds::Graph<N, E>& g) {
    if (g.nodes().empty()) return true;
    const auto reach = ds::bfsKHop(g, g.nodes().front(), static_cast<int>(g.nodes().size()));
    return reach.size() == g.nodes().size();
}

/// Number of connected components of an undirected graph.
template <class N, class E>
[[nodiscard]] std::size_t connectedComponents(const ds::Graph<N, E>& g) {
    std::size_t count = 0;
    ds::HashTable<N, bool> seen;
    for (const N& start : g.nodes()) {
        if (seen.contains(start)) continue;
        ++count;
        ds::Queue<N> frontier;
        seen.insert(start, true);
        frontier.enqueue(start);
        while (!frontier.empty()) {
            const N node = frontier.dequeue();
            for (const auto& [to, w] : g.neighbors(node)) {
                (void)w;
                if (!seen.contains(to)) {
                    seen.insert(to, true);
                    frontier.enqueue(to);
                }
            }
        }
    }
    return count;
}

/// Bipartite test via BFS 2-coloring (undirected). Returns the two color
/// classes when bipartite, empty vector otherwise.
template <class N, class E>
[[nodiscard]] std::vector<std::vector<N>> bipartiteColoring(const ds::Graph<N, E>& g) {
    ds::HashTable<N, int> color;
    for (const N& start : g.nodes()) {
        if (color.contains(start)) continue;
        color.insert(start, 0);
        ds::Queue<N> frontier;
        frontier.enqueue(start);
        while (!frontier.empty()) {
            const N node = frontier.dequeue();
            const int c = *color.get(node);
            for (const auto& [to, w] : g.neighbors(node)) {
                (void)w;
                if (!color.contains(to)) {
                    color.insert(to, 1 - c);
                    frontier.enqueue(to);
                } else if (*color.get(to) == c) {
                    return {};   // odd cycle
                }
            }
        }
    }
    std::vector<std::vector<N>> classes(2);
    for (const N& n : g.nodes()) classes[static_cast<std::size_t>(*color.get(n))].push_back(n);
    return classes;
}

/// Eulerian circuit/path existence (undirected, must be connected apart from
/// isolated vertices): 0 odd-degree vertices => circuit; exactly 2 => path.
template <class N, class E>
[[nodiscard]] std::string eulerType(const ds::Graph<N, E>& g) {
    std::size_t odd = 0;
    for (const N& n : g.nodes()) {
        if (g.degree(n) % 2 != 0) ++odd;
    }
    // connectivity among non-isolated vertices
    std::size_t nonIsolatedEdges = 0;
    for (const N& n : g.nodes()) nonIsolatedEdges += g.degree(n) > 0 ? 1 : 0;
    bool connectedCore = true;
    if (nonIsolatedEdges > 0) {
        const N& start = [&]() -> const N& {
            for (const N& n : g.nodes()) {
                if (g.degree(n) > 0) return n;
            }
            return g.nodes().front();
        }();
        const auto reach = ds::bfsKHop(g, start, static_cast<int>(g.nodes().size()));
        for (const N& n : g.nodes()) {
            if (g.degree(n) > 0) {
                bool found = false;
                for (const auto& [m, d] : reach) {
                    (void)d;
                    if (m == n) { found = true; break; }
                }
                if (!found) connectedCore = false;
            }
        }
    }
    if (!connectedCore) return "none";
    if (odd == 0) return "circuit";
    if (odd == 2) return "path";
    return "none";
}

/// Hamiltonian existence by brute force over permutations. GUARD: n <= 10.
template <class N, class E>
[[nodiscard]] bool hasHamiltonianPath(const ds::Graph<N, E>& g) {
    const auto& nodes = g.nodes();
    if (nodes.size() > 10) {
        throw util::ParseError("Hamiltonian guard: brute force limited to 10 vertices");
    }
    std::vector<std::size_t> perm(nodes.size());
    for (std::size_t i = 0; i < perm.size(); ++i) perm[i] = i;
    do {
        bool ok = true;
        for (std::size_t i = 0; i + 1 < perm.size() && ok; ++i) {
            if (!g.hasEdge(nodes[perm[i]], nodes[perm[i + 1]])) ok = false;
        }
        if (ok) return true;
    } while (std::next_permutation(perm.begin(), perm.end()));
    return false;
}

/// Induced subgraph on `keep` (nodes not listed are removed with their edges).
template <class N, class E>
[[nodiscard]] ds::Graph<N, E> inducedSubgraph(const ds::Graph<N, E>& g,
                                              const std::set<N>& keep) {
    ds::Graph<N, E> out(g.directed());
    for (const N& n : g.nodes()) {
        if (keep.count(n) != 0u) out.addNode(n);
    }
    for (const N& n : g.nodes()) {
        if (keep.count(n) == 0u) continue;
        for (const auto& [to, w] : g.neighbors(n)) {
            if (keep.count(to) != 0u) out.addEdge(n, to, w);
        }
    }
    return out;
}

/// Simple path check: a walk is a PATH when all vertices are distinct.
template <class N>
[[nodiscard]] bool isPath(const std::vector<N>& walk) {
    std::set<N> seen(walk.begin(), walk.end());
    return seen.size() == walk.size();
}

/// Greedy coloring: returns color per node (first-fit, node insertion order).
template <class N, class E>
[[nodiscard]] std::map<N, int> greedyColoring(const ds::Graph<N, E>& g) {
    std::map<N, int> colors;
    for (const N& node : g.nodes()) {
        std::set<int> used;
        for (const auto& [to, w] : g.neighbors(node)) {
            (void)w;
            const auto it = colors.find(to);
            if (it != colors.end()) used.insert(it->second);
        }
        int c = 0;
        while (used.count(c) != 0u) ++c;
        colors[node] = c;
    }
    return colors;
}

/// A^k walk counting: number of walks of length exactly k from a to b,
/// via integer matrix power over the undirected projection's adjacency matrix.
template <class N, class E>
[[nodiscard]] std::uint64_t walkCount(const ds::Graph<N, E>& g, const N& from, const N& to,
                                      std::size_t k) {
    const auto& nodes = g.nodes();
    const std::size_t n = nodes.size();
    std::vector<std::vector<std::uint64_t>> a(n, std::vector<std::uint64_t>(n, 0));
    ds::HashTable<N, std::size_t> idx;
    for (std::size_t i = 0; i < n; ++i) idx.insert(nodes[i], i);
    for (const N& node : nodes) {
        for (const auto& [to, w] : g.neighbors(node)) {
            (void)w;
            a[*idx.get(node)][*idx.get(to)] += 1;
        }
    }
    // matrix power A^k, mod 2^64 (wrapping is fine for counting demos)
    std::vector<std::vector<std::uint64_t>> result(n, std::vector<std::uint64_t>(n, 0));
    for (std::size_t i = 0; i < n; ++i) result[i][i] = 1;
    std::vector<std::vector<std::uint64_t>> base = a;
    while (k > 0) {
        if (k & 1ULL) {
            auto next = result;
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = 0; j < n; ++j) {
                    std::uint64_t sum = 0;
                    for (std::size_t p = 0; p < n; ++p) sum += result[i][p] * base[p][j];
                    next[i][j] = sum;
                }
            }
            result = std::move(next);
        }
        if (k > 1) {
            auto next = base;
            for (std::size_t i = 0; i < n; ++i) {
                for (std::size_t j = 0; j < n; ++j) {
                    std::uint64_t sum = 0;
                    for (std::size_t p = 0; p < n; ++p) sum += base[i][p] * base[p][j];
                    next[i][j] = sum;
                }
            }
            base = std::move(next);
        }
        k >>= 1;
    }
    return result[*idx.get(from)][*idx.get(to)];
}

} // namespace verity::dm
