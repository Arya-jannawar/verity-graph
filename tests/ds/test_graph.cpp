// VERITY GRAPH — tests/ds/test_graph.cpp
// Graph: BFS k-hop + shortest path vs hand-computed expectation and a
// brute-force oracle, DFS recursive==iterative on random graphs, transpose,
// degree conventions, parallel edges/self-loops, error paths.

#include "doctest/doctest.h"

#include <algorithm>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/GraphAlgo.hpp"
#include "verity/util/VerityError.hpp"

namespace {

/// Brute-force shortest path (enumerating all simple paths up to depth V by
/// BFS with std::set oracle) on small graphs.
std::size_t oracleShortestDistance(const verity::ds::Graph<std::string, int>& g,
                                   const std::string& a, const std::string& b) {
    if (a == b) return 0;
    std::set<std::string> visited{a};
    std::vector<std::pair<std::string, int>> frontier{{a, 0}};
    while (!frontier.empty()) {
        auto [node, d] = frontier.front();
        frontier.erase(frontier.begin());
        for (const auto& [nb, w] : g.neighbors(node)) {
            (void)w;
            if (nb == b) return static_cast<std::size_t>(d + 1);
            if (!visited.count(nb)) {
                visited.insert(nb);
                frontier.emplace_back(nb, d + 1);
            }
        }
    }
    return static_cast<std::size_t>(-1);
}

void buildRandomGraph(verity::ds::Graph<std::string, int>& g, std::mt19937& rng,
                      int nodeCount, int edgeCount) {
    for (int i = 0; i < nodeCount; ++i) g.addNode("n" + std::to_string(i));
    for (int i = 0; i < edgeCount; ++i) {
        const int a = static_cast<int>(rng() % static_cast<unsigned>(nodeCount));
        const int b = static_cast<int>(rng() % static_cast<unsigned>(nodeCount));
        g.addEdge("n" + std::to_string(a), "n" + std::to_string(b),
                  static_cast<int>(rng() % 10));
    }
}

} // namespace

TEST_CASE("Graph: fixed 6-node BFS k-hop and shortest path") {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C", "D", "E", "F"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("A", "C", 1);
    g.addEdge("B", "D", 1);
    g.addEdge("C", "D", 1);
    g.addEdge("D", "E", 1);
    // F is disconnected

    const auto hop1 = verity::ds::bfsKHop(g, std::string("A"), 1);
    CHECK_EQ(hop1.size(), 3u);  // A, B, C
    const auto hop3 = verity::ds::bfsKHop(g, std::string("A"), 3);
    CHECK_EQ(hop3.size(), 5u);  // A, B, C, D, E (F unreachable)
    int depthE = -1;
    for (const auto& [n, d] : hop3) {
        if (n == "E") depthE = d;
    }
    CHECK_EQ(depthE, 3);        // A -> B -> D -> E
    const auto hop2 = verity::ds::bfsKHop(g, std::string("A"), 2);
    CHECK_EQ(hop2.size(), 4u);  // E needs 3 hops; excluded at k=2

    const auto path = verity::ds::bfsShortestPath(g, std::string("A"), std::string("E"));
    REQUIRE_EQ(path.size(), 4u);
    CHECK_EQ(path.front(), "A");
    CHECK_EQ(path.back(), "E");
    CHECK_EQ(oracleShortestDistance(g, "A", "E"), 3u);

    const auto none = verity::ds::bfsShortestPath(g, std::string("A"), std::string("F"));
    CHECK(none.empty());
}

TEST_CASE("Graph: BFS shortest path vs brute-force oracle on random graphs") {
    std::mt19937 rng(5150);
    for (int trial = 0; trial < 30; ++trial) {
        verity::ds::Graph<std::string, int> g(true);
        buildRandomGraph(g, rng, 8, 14);
        const auto nodes = g.nodes();
        for (const auto& a : nodes) {
            for (const auto& b : nodes) {
                const auto path = verity::ds::bfsShortestPath(g, a, b);
                const std::size_t dist = oracleShortestDistance(g, a, b);
                if (dist == static_cast<std::size_t>(-1)) {
                    CHECK(path.empty());
                } else {
                    REQUIRE_FALSE(path.empty());
                    CHECK_EQ(path.size() - 1, dist);
                    CHECK_EQ(path.front(), a);
                    CHECK_EQ(path.back(), b);
                    // verify path edges actually exist
                    for (std::size_t i = 0; i + 1 < path.size(); ++i) {
                        CHECK(g.hasEdge(path[i], path[i + 1]));
                    }
                }
            }
        }
    }
}

TEST_CASE("Graph: dfsIterative equals dfsRecursive on random graphs") {
    std::mt19937 rng(777);
    for (int trial = 0; trial < 30; ++trial) {
        verity::ds::Graph<std::string, int> g(true);
        buildRandomGraph(g, rng, 10, 18);
        for (const auto& n : g.nodes()) {
            const auto rec = verity::ds::dfsRecursive(g, n);
            const auto iter = verity::ds::dfsIterative(g, n);
            REQUIRE_EQ(rec.size(), iter.size());
            CHECK(rec == iter);
        }
    }
}

TEST_CASE("Graph: transpose reverses every edge exactly") {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C"}) g.addNode(n);
    g.addEdge("A", "B", 5);
    g.addEdge("B", "C", 7);
    g.addEdge("C", "A", 9);
    auto t = g.transpose();
    CHECK(t.hasEdge("B", "A"));
    CHECK(t.hasEdge("C", "B"));
    CHECK(t.hasEdge("A", "C"));
    CHECK_FALSE(t.hasEdge("A", "B"));
    CHECK_EQ(t.edgeCount(), 3u);
    CHECK_EQ(t[{"A", "C"}], 9);   // original A->C? no: t has C->A(9); check
}

TEST_CASE("Graph: degree conventions, self-loops, parallel edges") {
    verity::ds::Graph<std::string, int> u(false);
    for (const char* n : {"A", "B"}) u.addNode(n);
    u.addEdgeUndirected("A", "B", 1);
    CHECK_EQ(u.degree("A"), 1u);
    CHECK_EQ(u.degree("B"), 1u);
    u.addEdgeUndirected("A", "A", 2);   // self-loop
    CHECK_EQ(u.degree("A"), 3u);        // loop counts 2 + edge to B
    CHECK_EQ(u.edgeCount(), 2u);        // pair (A,B) + loop, each once

    // parallel edges
    u.addEdgeUndirected("A", "B", 3);
    CHECK_EQ(u.degree("A"), 4u);
    CHECK(u.hasEdge("A", "B"));
    CHECK_EQ(u.edgeCount(), 3u);
    CHECK(u.removeEdge("A", "B"));      // removes first match
    CHECK(u.hasEdge("A", "B"));         // second parallel copy remains
    CHECK_EQ(u.edgeCount(), 2u);

    verity::ds::Graph<std::string, int> d(true);
    for (const char* n : {"A", "B"}) d.addNode(n);
    d.addEdge("A", "B", 1);
    d.addEdge("B", "A", 1);
    d.addEdge("A", "A", 1);             // directed self-loop
    CHECK_EQ(d.outDegree("A"), 2u);
    CHECK_EQ(d.inDegree("A"), 2u);      // loop appears in- and out-
    CHECK_EQ(d.degree("A"), 4u);
    CHECK_EQ(d.edgeCount(), 3u);
}

TEST_CASE("Graph: removeNode cleans edges and reindexes") {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C", "D"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("B", "C", 1);
    g.addEdge("C", "D", 1);
    g.addEdge("D", "A", 1);
    CHECK(g.removeNode("B"));
    CHECK_FALSE(g.hasNode("B"));
    CHECK_EQ(g.nodeCount(), 3u);
    CHECK_EQ(g.edgeCount(), 2u);        // A->B and B->C gone
    CHECK(g.hasEdge("C", "D"));
    CHECK(g.hasEdge("D", "A"));
    // queries on remaining nodes still resolve (reindex check)
    const auto path = verity::ds::bfsShortestPath(g, std::string("C"), std::string("A"));
    REQUIRE_EQ(path.size(), 3u);
    CHECK(g.removeNode("Z") == false);
}

TEST_CASE("Graph: error paths — Duplicate/MissingNodeError") {
    verity::ds::Graph<std::string, int> g(true);
    g.addNode("A");
    CHECK_THROWS_AS(g.addNode("A"), verity::util::VerityError);
    CHECK_THROWS_AS((void)g.neighbors("ghost"), verity::util::VerityError);
    CHECK_THROWS_AS((void)g.outDegree("ghost"), verity::util::VerityError);
    CHECK_THROWS_AS(g.addEdge("A", "ghost", 1), verity::util::VerityError);
    const std::pair<std::string, std::string> missing{"A", "ghost"};
    CHECK_THROWS_AS((void)g[missing], verity::util::VerityError);
}
