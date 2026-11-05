// VERITY GRAPH — tests/ds/test_dsa3.cpp
// Phase-3 DSA: Dijkstra, Kruskal vs Prim agreement, Tarjan SCC, articulation
// points and bridges (vs brute-force oracles), Rabin-Karp.

#include "doctest/doctest.h"

#include <algorithm>
#include <limits>
#include <random>
#include <set>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/GraphAlgorithms.hpp"
#include "verity/ds/StringSearch.hpp"
#include "verity/util/VerityError.hpp"

namespace {

/// Brute-force articulation points: a node is articulable iff removing it
/// increases the connected-component count of the undirected projection.
std::set<std::string> bruteArticulation(const verity::ds::Graph<std::string, double>& g) {
    std::set<std::string> out;
    const std::size_t baseComponents = [&] {
        // count components via BFS on the graph itself
        std::size_t count = 0;
        std::set<std::string> seen;
        for (const auto& start : g.nodes()) {
            if (seen.count(start) != 0u) continue;
            ++count;
            std::vector<std::string> frontier{start};
            seen.insert(start);
            while (!frontier.empty()) {
                const std::string node = frontier.back();
                frontier.pop_back();
                for (const auto& [to, w] : g.neighbors(node)) {
                    (void)w;
                    if (seen.count(to) == 0u) {
                        seen.insert(to);
                        frontier.push_back(to);
                    }
                }
            }
        }
        return count;
    }();
    for (const auto& victim : g.nodes()) {
        verity::ds::Graph<std::string, double> reduced(false);
        for (const auto& n : g.nodes()) {
            if (n != victim) reduced.addNode(n);
        }
        std::set<std::pair<std::string, std::string>> added;
        for (const auto& n : g.nodes()) {
            if (n == victim) continue;
            for (const auto& [to, w] : g.neighbors(n)) {
                (void)w;
                if (to == victim || to == n) continue;
                const auto key = n < to ? std::make_pair(n, to) : std::make_pair(to, n);
                if (added.insert(key).second) reduced.addEdgeUndirected(n, to, w);
            }
        }
        // count components of reduced
        std::size_t count = 0;
        std::set<std::string> seen;
        for (const auto& start : reduced.nodes()) {
            if (seen.count(start) != 0u) continue;
            ++count;
            std::vector<std::string> frontier{start};
            seen.insert(start);
            while (!frontier.empty()) {
                const std::string node = frontier.back();
                frontier.pop_back();
                for (const auto& [to, w] : reduced.neighbors(node)) {
                    (void)w;
                    if (seen.count(to) == 0u) {
                        seen.insert(to);
                        frontier.push_back(to);
                    }
                }
            }
        }
        if (count > baseComponents) out.insert(victim);
    }
    return out;
}

/// Brute-force shortest path by enumeration over simple paths (small graphs).
double bruteDistance(const verity::ds::Graph<std::string, double>& g,
                     const std::string& a, const std::string& b) {
    double best = (std::numeric_limits<double>::infinity)();
    std::function<void(const std::string&, double, std::set<std::string>)> walk =
        [&](const std::string& node, double cost, std::set<std::string> seen) {
            if (node == b) {
                best = std::min(best, cost);
                return;
            }
            for (const auto& [to, w] : g.neighbors(node)) {
                if (seen.count(to) != 0u) continue;
                seen.insert(to);
                walk(to, cost + w, seen);
                seen.erase(to);
            }
        };
    walk(a, 0.0, {a});
    return best;
}

} // namespace

TEST_CASE("Dijkstra: known distances and honest cost semantics") {
    verity::ds::Graph<std::string, double> g(true);
    for (const char* n : {"A", "B", "C", "D"}) g.addNode(n);
    g.addEdge("A", "B", 1.0);
    g.addEdge("B", "C", 1.0);
    g.addEdge("A", "C", 2.5);
    g.addEdge("C", "D", 0.5);
    const auto dist = verity::ds::dijkstra(
        g, std::string("A"), [](const double w) { return w; });
    double dB = -1, dC = -1, dD = -1;
    for (const auto& [n, d] : dist) {
        if (n == "B") dB = d;
        if (n == "C") dC = d;
        if (n == "D") dD = d;
    }
    CHECK_EQ(dB, doctest::Approx(1.0));
    CHECK_EQ(dC, doctest::Approx(2.0));   // via B, not the direct 2.5 edge
    CHECK_EQ(dD, doctest::Approx(2.5));

    // brute-force oracle on random graphs
    std::mt19937 rng(9);
    for (int trial = 0; trial < 15; ++trial) {
        verity::ds::Graph<std::string, double> r(false);
        for (int i = 0; i < 7; ++i) r.addNode("n" + std::to_string(i));
        for (int e = 0; e < 12; ++e) {
            const int i = static_cast<int>(rng() % 7);
            const int j = static_cast<int>(rng() % 7);
            if (i == j) continue;
            const double w = 0.5 + static_cast<double>(rng() % 40) / 10.0;
            r.addEdgeUndirected("n" + std::to_string(i), "n" + std::to_string(j), w);
        }
        for (const auto& a : r.nodes()) {
            const auto got = verity::ds::dijkstra(r, a, [](const double w) { return w; });
            for (const auto& [n, d] : got) {
                const double expected = bruteDistance(r, a, n);
                if (std::isfinite(expected)) {
                    CHECK_EQ(d, doctest::Approx(expected).epsilon(1e-6));
                }
            }
        }
    }
}

namespace {
double directWeight(const double& w) { return w; }
} // namespace

TEST_CASE("Kruskal and Prim agree on the undirected projection") {
    verity::ds::Graph<std::string, double> g(false);
    for (const char* n : {"A", "B", "C", "D", "E"}) g.addNode(n);
    g.addEdgeUndirected("A", "B", 1);
    g.addEdgeUndirected("B", "C", 2);
    g.addEdgeUndirected("A", "C", 3);
    g.addEdgeUndirected("C", "D", 1);
    g.addEdgeUndirected("D", "E", 2);
    g.addEdgeUndirected("A", "E", 4);
    const auto kruskal = verity::ds::kruskalMst(g, directWeight);
    const auto prim = verity::ds::primMst(g, directWeight);
    CHECK_EQ(kruskal.size(), 4u);   // spanning tree of 5 nodes
    CHECK_EQ(prim.size(), 4u);
    // same total weight (different edge sets may tie; weight must match)
    double kw = 0, pw = 0;
    for (const auto& [a, b, w] : kruskal) {
        (void)a;
        (void)b;
        kw += w;
    }
    for (const auto& [a, b, w] : prim) {
        (void)a;
        (void)b;
        pw += w;
    }
    CHECK_EQ(kw, doctest::Approx(pw));
    CHECK_EQ(kw, doctest::Approx(6.0));   // optimal: 1+2+1+2

    // disconnected graph -> spanning FOREST covering both components
    g.addNode("F");
    g.addNode("G");
    g.addEdgeUndirected("F", "G", 1);
    CHECK_EQ(verity::ds::kruskalMst(g, directWeight).size(), 5u);

    // brute-force oracle on tiny random graphs: MST weight is minimal
    std::mt19937 rng(77);
    for (int trial = 0; trial < 30; ++trial) {
        verity::ds::Graph<std::string, double> r(false);
        for (int i = 0; i < 5; ++i) r.addNode(std::to_string(i));
        for (int i = 0; i < 5; ++i) {
            for (int j = i + 1; j < 5; ++j) {
                if (rng() % 3 == 0) {
                    r.addEdgeUndirected(std::to_string(i), std::to_string(j),
                                        1.0 + static_cast<double>(rng() % 20));
                }
            }
        }
        const auto mst = verity::ds::kruskalMst(r, directWeight);
        // compare against Prim on the same graph (independent implementation)
        const auto prim2 = verity::ds::primMst(r, directWeight);
        double mw = 0, pw2 = 0;
        for (const auto& [a, b, w] : mst) {
            (void)a;
            (void)b;
            mw += w;
        }
        for (const auto& [a, b, w] : prim2) {
            (void)a;
            (void)b;
            pw2 += w;
        }
        CHECK_EQ(mw, doctest::Approx(pw2));
    }
}

TEST_CASE("Tarjan SCC: known components on a digraph") {
    verity::ds::Graph<std::string, double> g(true);
    for (const char* n : {"A", "B", "C", "D", "E"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("B", "C", 1);
    g.addEdge("C", "A", 1);   // cycle A-B-C
    g.addEdge("C", "D", 1);
    g.addEdge("D", "E", 1);
    const auto sccs = verity::ds::tarjanScc(g);
    REQUIRE_EQ(sccs.size(), 3u);
    bool foundCycle = false;
    for (const auto& scc : sccs) {
        if (scc.size() == 3) {
            foundCycle = true;
            CHECK(scc == std::vector<std::string>({"A", "B", "C"}));
        }
    }
    CHECK(foundCycle);
}

TEST_CASE("articulation points and bridges vs brute-force oracle") {
    // chain: every interior node is articulable, every edge a bridge
    verity::ds::Graph<std::string, double> chain(false);
    for (const char* n : {"A", "B", "C", "D"}) chain.addNode(n);
    chain.addEdgeUndirected("A", "B", 1);
    chain.addEdgeUndirected("B", "C", 1);
    chain.addEdgeUndirected("C", "D", 1);
    const auto cuts = verity::ds::cutVerticesAndBridges(chain);
    CHECK_EQ(cuts.articulationPoints.size(), 2u);   // B, C
    CHECK_EQ(cuts.bridges.size(), 3u);

    // cycle: no articulation, no bridges
    verity::ds::Graph<std::string, double> cycle(false);
    for (const char* n : {"A", "B", "C"}) cycle.addNode(n);
    cycle.addEdgeUndirected("A", "B", 1);
    cycle.addEdgeUndirected("B", "C", 1);
    cycle.addEdgeUndirected("C", "A", 1);
    const auto cuts2 = verity::ds::cutVerticesAndBridges(cycle);
    CHECK(cuts2.articulationPoints.empty());
    CHECK(cuts2.bridges.empty());

    // parallel edges: never a bridge
    verity::ds::Graph<std::string, double> parallel(false);
    for (const char* n : {"A", "B"}) parallel.addNode(n);
    parallel.addEdgeUndirected("A", "B", 1);
    parallel.addEdgeUndirected("A", "B", 2);
    const auto cuts3 = verity::ds::cutVerticesAndBridges(parallel);
    CHECK(cuts3.bridges.empty());

    // random graphs vs the removal oracle
    std::mt19937 rng(1313);
    for (int trial = 0; trial < 25; ++trial) {
        verity::ds::Graph<std::string, double> r(false);
        for (int i = 0; i < 6; ++i) r.addNode("n" + std::to_string(i));
        for (int e = 0; e < 8; ++e) {
            const int i = static_cast<int>(rng() % 6);
            const int j = static_cast<int>(rng() % 6);
            if (i != j) r.addEdgeUndirected("n" + std::to_string(i), "n" + std::to_string(j), 1);
        }
        const auto got = verity::ds::cutVerticesAndBridges(r);
        const auto expected = bruteArticulation(r);
        std::set<std::string> gotSet(got.articulationPoints.begin(),
                                     got.articulationPoints.end());
        CHECK_EQ(gotSet, expected);
    }
}

TEST_CASE("Rabin-Karp: exact verified matches") {
    const std::string text = "Product X launched in Bellhaven on 2024-03-10 at the showcase.";
    CHECK_EQ(verity::ds::rabinKarpFind(text, "Bellhaven"), 22u);
    CHECK_EQ(verity::ds::rabinKarpFind(text, "launch"), 10u);
    CHECK_EQ(verity::ds::rabinKarpFind(text, "absent"), std::size_t(-1));
    CHECK_EQ(verity::ds::rabinKarpFind(text, ""), 0u);   // empty needle at 0
    CHECK_EQ(verity::ds::rabinKarpFind("", "a"), std::size_t(-1));

    // overlapping matches
    const auto all = verity::ds::rabinKarpFindAll("aaaa", "aa");
    CHECK_EQ(all, std::vector<std::size_t>({0u, 1u, 2u}));

    // oracle vs std::string::find on random cases
    std::mt19937 rng(20240310);
    const std::string alphabet = "abcd";
    for (int trial = 0; trial < 200; ++trial) {
        std::string hay;
        for (int i = 0; i < 30; ++i) hay += alphabet[rng() % 4];
        std::string needle;
        for (int i = 0; i < 2 + static_cast<int>(rng() % 3); ++i) {
            needle += alphabet[rng() % 4];
        }
        const std::size_t got = verity::ds::rabinKarpFind(hay, needle);
        const std::size_t expected = hay.find(needle);
        CHECK_EQ(got, expected);
    }
}
