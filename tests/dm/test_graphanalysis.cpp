// VERITY GRAPH — tests/dm/test_graphanalysis.cpp
// DM Unit 5 graph theory: degree sequence, handshake (undirected + digraph),
// isolated/pendant vertices, connectedness, bipartite 2-coloring, Euler type,
// guarded Hamiltonian, induced subgraph, greedy coloring, A^k walks, and the
// noisy-OR monoid laws.

#include "doctest/doctest.h"

#include <cmath>
#include <numeric>
#include <random>
#include <vector>

#include "verity/dm/GraphAnalysis.hpp"
#include "verity/dm/NoisyOr.hpp"
#include "verity/util/VerityError.hpp"

namespace {

verity::ds::Graph<std::string, double> cycle4() {
    verity::ds::Graph<std::string, double> g(false);
    for (const char* n : {"A", "B", "C", "D"}) g.addNode(n);
    g.addEdgeUndirected("A", "B", 1);
    g.addEdgeUndirected("B", "C", 1);
    g.addEdgeUndirected("C", "D", 1);
    g.addEdgeUndirected("D", "A", 1);
    return g;
}

} // namespace

TEST_CASE("degree sequence and handshake (undirected and digraph)") {
    const auto g = cycle4();
    CHECK_EQ(verity::dm::degreeSequence(g), std::vector<std::size_t>({2, 2, 2, 2}));
    CHECK(verity::dm::handshakeHolds(g));
    CHECK_EQ(g.edgeCount(), 4u);

    // add a self-loop: degree counts 2, edgeCount 1 -> sum == 2|E| still holds
    g.addEdgeUndirected("A", "A", 1);
    CHECK(verity::dm::handshakeHolds(g));
    CHECK_EQ(g.degree("A"), 4u);   // 2 (loop) + 2 (A-B, D-A)

    // digraph handshake: sum(in) == sum(out) == |A|
    verity::ds::Graph<std::string, double> d(true);
    for (const char* n : {"X", "Y", "Z"}) d.addNode(n);
    d.addEdge("X", "Y", 1);
    d.addEdge("Y", "Z", 1);
    d.addEdge("Z", "X", 1);
    CHECK(verity::dm::digraphHandshakeHolds(d));
    CHECK_EQ(d.edgeCount(), 3u);
}

TEST_CASE("isolated, pendant, connected components") {
    verity::ds::Graph<std::string, double> g(false);
    for (const char* n : {"A", "B", "C", "D", "E"}) g.addNode(n);
    g.addEdgeUndirected("A", "B", 1);
    g.addEdgeUndirected("B", "C", 1);
    // D isolated, E pendant
    g.addEdgeUndirected("C", "E", 1);
    const auto isolated = verity::dm::isolatedVertices(g);
    const auto pendant = verity::dm::pendantVertices(g);
    CHECK_EQ(isolated.size(), 1u);
    CHECK_EQ(isolated[0], "D");
    CHECK_EQ(pendant.size(), 2u);   // A and E both degree 1
    CHECK_EQ(verity::dm::connectedComponents(g), 2u);
    CHECK_FALSE(verity::dm::isConnected(g));
}

TEST_CASE("bipartite: even cycle yes, odd cycle no") {
    const auto even = cycle4();
    const auto classes = verity::dm::bipartiteColoring(even);
    CHECK_EQ(classes.size(), 2u);   // two non-empty color classes

    verity::ds::Graph<std::string, double> odd(false);
    for (const char* n : {"A", "B", "C"}) odd.addNode(n);
    odd.addEdgeUndirected("A", "B", 1);
    odd.addEdgeUndirected("B", "C", 1);
    odd.addEdgeUndirected("C", "A", 1);
    CHECK(verity::dm::bipartiteColoring(odd).empty());
}

TEST_CASE("Euler existence") {
    const auto circuit = cycle4();
    CHECK_EQ(verity::dm::eulerType(circuit), std::string("circuit"));
    circuit.addEdgeUndirected("A", "C", 1);   // A:3, C:3, B:2, D:2 -> exactly 2 odd
    CHECK_EQ(verity::dm::eulerType(circuit), std::string("path"));
    verity::ds::Graph<std::string, double> path(false);
    for (const char* n : {"A", "B", "C", "D"}) path.addNode(n);
    path.addEdgeUndirected("A", "B", 1);
    path.addEdgeUndirected("B", "C", 1);
    path.addEdgeUndirected("C", "D", 1);   // exactly 2 odd vertices
    CHECK_EQ(verity::dm::eulerType(path), std::string("path"));
    path.addEdgeUndirected("D", "A", 1);   // all degree 2 -> circuit again
    CHECK_EQ(verity::dm::eulerType(path), std::string("circuit"));
}

TEST_CASE("Hamiltonian brute force: cycle yes, path-missing graph no") {
    const auto g = cycle4();
    CHECK(verity::dm::hasHamiltonianPath(g));
    verity::ds::Graph<std::string, double> star(false);
    for (const char* n : {"A", "B", "C", "D"}) star.addNode(n);
    star.addEdgeUndirected("A", "B", 1);
    star.addEdgeUndirected("A", "C", 1);
    star.addEdgeUndirected("A", "D", 1);
    CHECK_FALSE(verity::dm::hasHamiltonianPath(star));
    // guard: an 11-vertex graph throws instead of exploding
    verity::ds::Graph<std::string, double> big(false);
    for (int i = 0; i < 11; ++i) big.addNode("v" + std::to_string(i));
    CHECK_THROWS_AS((void)verity::dm::hasHamiltonianPath(big), verity::util::ParseError);
}

TEST_CASE("induced subgraph and greedy coloring") {
    const auto g = cycle4();
    const auto induced = verity::dm::inducedSubgraph(g, std::set<std::string>{"A", "B", "C"});
    CHECK_EQ(induced.nodeCount(), 3u);
    CHECK_EQ(induced.edgeCount(), 2u);
    const auto colors = verity::dm::greedyColoring(g);
    // A and C share no edge; B and D share no edge -> 2 colors suffice
    int maxColor = 0;
    for (const auto& [n, c] : colors) maxColor = std::max(maxColor, c);
    CHECK_EQ(maxColor, 1);
}

TEST_CASE("A^k walk counting (walks may repeat vertices)") {
    const auto g = cycle4();
    // length-1 walks: direct edges only
    CHECK_EQ(verity::dm::walkCount(g, std::string("A"), std::string("B"), 1), 1u);
    CHECK_EQ(verity::dm::walkCount(g, std::string("A"), std::string("C"), 1), 0u);
    // length-2 walks A->C: A-B-C and A-D-C
    CHECK_EQ(verity::dm::walkCount(g, std::string("A"), std::string("C"), 2), 2u);
    // length-4 walks A->A on C4: brute force counts 6 (A^4 diagonal entry)
    CHECK_EQ(verity::dm::walkCount(g, std::string("A"), std::string("A"), 4), 8u);  // eigenvalue check: (2^4+0^4+(-2)^4+0^4)/4 = 8
}

TEST_CASE("noisy-OR: monoid laws over [0,1] with identity 0 (fp tolerance)") {
    const auto combine = [](double a, double b) { return verity::dm::noisyOrCombine(a, b); };
    const double tol = 1e-12;
    const std::vector<double> elems{0.0, 0.2, 0.5, 0.7, 1.0};
    // commutativity (exact in fp for two operands)
    for (const double a : elems) {
        for (const double b : elems) {
            CHECK_EQ(combine(a, b), doctest::Approx(combine(b, a)));
            CHECK((combine(a, b) >= a - tol && combine(a, b) >= b - tol));
        }
    }
    // associativity within floating-point tolerance
    for (const double a : elems) {
        for (const double b : elems) {
            for (const double c : elems) {
                CHECK(std::abs(combine(combine(a, b), c) - combine(a, combine(b, c))) < 1e-12);
            }
        }
    }
    // identity 0 both sides
    for (const double a : elems) {
        CHECK_EQ(combine(0.0, a), doctest::Approx(a));
        CHECK_EQ(combine(a, 0.0), doctest::Approx(a));
    }
    // merge order never changes the aggregate
    const std::vector<double> seq1{0.3, 0.4, 0.5};
    const std::vector<double> seq2{0.5, 0.3, 0.4};
    CHECK(std::abs(verity::dm::noisyOrAggregate(seq1) - verity::dm::noisyOrAggregate(seq2)) < 1e-12);

    // inputs outside [0,1] throw
    CHECK_THROWS_AS((void)verity::dm::noisyOrCombine(1.2, 0.1), verity::util::ParseError);
}

TEST_CASE("noisy-OR evidence aggregation: dedup and circular halving") {
    using verity::dm::EvidenceSource;
    using verity::dm::noisyOrAggregateEvidence;
    // same document twice counts ONCE
    const std::vector<EvidenceSource> dedup{{"D001", 0.5, false}, {"D001", 0.5, false}};
    CHECK_EQ(noisyOrAggregateEvidence(dedup), doctest::Approx(0.5));
    // two independent documents combine
    const std::vector<EvidenceSource> two{{"D001", 0.5, false}, {"D002", 0.5, false}};
    CHECK_EQ(noisyOrAggregateEvidence(two), doctest::Approx(0.75));
    // circular source is halved before aggregation
    const std::vector<EvidenceSource> circ{{"D001", 0.5, true}};
    CHECK_EQ(noisyOrAggregateEvidence(circ), doctest::Approx(0.25));
}
