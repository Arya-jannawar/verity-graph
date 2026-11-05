// VERITY GRAPH — tests/ds/test_topo.cpp
// Topological sort (Kahn == DFS on DAGs), cycle detection (directed witness,
// undirected DSU) on known graphs and random oracles.

#include "doctest/doctest.h"

#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/TopoSort.hpp"
#include "verity/util/VerityError.hpp"

namespace {

verity::ds::Graph<std::string, int> makeChain() {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C", "D"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("B", "C", 1);
    g.addEdge("C", "D", 1);
    return g;
}

} // namespace

TEST_CASE("kahnTopoSort: chain produces the unique order; DFS matches") {
    const auto g = makeChain();
    const auto kahn = verity::ds::kahnTopoSort(g);
    const auto dfs = verity::ds::dfsTopoSort(g);
    CHECK_EQ(kahn, std::vector<std::string>({"A", "B", "C", "D"}));
    CHECK_EQ(dfs, std::vector<std::string>({"A", "B", "C", "D"}));
}

TEST_CASE("kahnTopoSort: diamond has exactly the two valid orders") {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C", "D"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("A", "C", 1);
    g.addEdge("B", "D", 1);
    g.addEdge("C", "D", 1);
    const auto order = verity::ds::kahnTopoSort(g);
    REQUIRE_EQ(order.size(), 4u);
    CHECK_EQ(order.front(), "A");
    CHECK_EQ(order.back(), "D");
    // position check: B and C occupy slots 1-2 in some order
    const std::set<std::string> middle{order[1], order[2]};
    CHECK_EQ(middle, std::set<std::string>({"B", "C"}));
    // DFS order is a valid topological order too: verify against a set oracle
    const auto dfs = verity::ds::dfsTopoSort(g);
    REQUIRE_EQ(dfs.size(), 4u);
    std::vector<std::size_t> pos(4);
    for (std::size_t i = 0; i < 4; ++i) {
        pos[static_cast<std::size_t>(dfs[i][0] - 'A')] = i;
    }
    CHECK(pos[0] < pos[1]);   // A before B
    CHECK(pos[0] < pos[2]);   // A before C
    CHECK(pos[1] < pos[3]);   // B before D
    CHECK(pos[2] < pos[3]);   // C before D
}

TEST_CASE("topo sort returns EMPTY on a cycle (withholds output)") {
    verity::ds::Graph<std::string, int> g(true);
    for (const char* n : {"A", "B", "C"}) g.addNode(n);
    g.addEdge("A", "B", 1);
    g.addEdge("B", "C", 1);
    g.addEdge("C", "A", 1);
    CHECK(verity::ds::kahnTopoSort(g).empty());
    CHECK(verity::ds::dfsTopoSort(g).empty());
    // witness: A -> B -> C -> A
    const auto witness = verity::ds::directedCycle(g);
    REQUIRE_EQ(witness.size(), 4u);
    CHECK_EQ(witness.front(), witness.back());
    const std::set<std::string> nodes(witness.begin(), witness.end());
    CHECK_EQ(nodes.size(), 3u);
}

TEST_CASE("directedCycle: self-loop is a cycle") {
    verity::ds::Graph<std::string, int> g(true);
    g.addNode("A");
    g.addNode("B");
    g.addEdge("A", "A", 1);
    const auto witness = verity::ds::directedCycle(g);
    REQUIRE_EQ(witness.size(), 2u);
    CHECK_EQ(witness[0], "A");
    CHECK_EQ(witness[1], "A");
}

TEST_CASE("undirectedHasCycle: acyclic tree vs cycle graph") {
    verity::ds::Graph<std::string, int> tree(false);
    for (const char* n : {"A", "B", "C", "D"}) tree.addNode(n);
    tree.addEdgeUndirected("A", "B", 1);
    tree.addEdgeUndirected("B", "C", 1);
    tree.addEdgeUndirected("B", "D", 1);
    CHECK_FALSE(verity::ds::undirectedHasCycle(tree));
    tree.addEdgeUndirected("D", "A", 1);   // closes the loop
    CHECK(verity::ds::undirectedHasCycle(tree));
}

TEST_CASE("random DAGs: Kahn and DFS both produce valid topological orders") {
    std::mt19937 rng(2718);
    for (int trial = 0; trial < 40; ++trial) {
        verity::ds::Graph<int, int> g(true);
        const int n = 3 + static_cast<int>(rng() % 6);
        for (int i = 0; i < n; ++i) g.addNode(i);
        // edges only from lower to higher => guaranteed acyclic
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (rng() % 3 == 0) g.addEdge(i, j, 1);
            }
        }
        const auto kahn = verity::ds::kahnTopoSort(g);
        const auto dfs = verity::ds::dfsTopoSort(g);
        REQUIRE_EQ(kahn.size(), g.nodes().size());
        REQUIRE_EQ(dfs.size(), g.nodes().size());
        // verify ordering constraints
        std::vector<int> position(static_cast<std::size_t>(n));
        for (int which = 0; which < 2; ++which) {
            const auto& order = which == 0 ? kahn : dfs;
            for (std::size_t p = 0; p < order.size(); ++p) {
                position[static_cast<std::size_t>(order[p])] = static_cast<int>(p);
            }
            for (int i = 0; i < n; ++i) {
                for (const auto& [to, w] : g.neighbors(i)) {
                    (void)w;
                    CHECK(position[static_cast<std::size_t>(i)] <
                          position[static_cast<std::size_t>(to)]);
                }
            }
        }
    }
}
