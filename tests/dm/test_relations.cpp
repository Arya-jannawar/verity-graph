// VERITY GRAPH — tests/dm/test_relations.cpp
// Relation<T>: properties, closures (Warshall vs brute force), equivalence
// classes vs DisjointSet, Hasse reduction, bounds/lattice, incomparables,
// partition checks, inverse/composition.

#include "doctest/doctest.h"

#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "verity/dm/Relations.hpp"
#include "verity/ds/DisjointSet.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::Relation;

namespace {

/// Brute-force transitive closure: repeatedly compose the ACCUMULATED set
/// with itself until fixpoint (composing only the original edges could never
/// produce paths longer than two hops).
std::set<std::pair<int, int>> bruteClosure(const std::set<std::pair<int, int>>& edges,
                                           const std::vector<int>& elems) {
    std::set<std::pair<int, int>> out = edges;
    bool changed = true;
    while (changed) {
        changed = false;
        const auto snapshot = out;
        for (const auto& [a, b] : snapshot) {
            for (const auto& [c, d] : snapshot) {
                if (b == c && out.insert({a, d}).second) changed = true;
            }
        }
    }
    (void)elems;
    return out;
}

} // namespace

TEST_CASE("Relation: property checks on known relations") {
    // divides on {1,2,3,4}: reflexive antisymmetric transitive partial order
    Relation<int> le(std::vector<int>{1, 2, 3, 4});
    for (const int a : {1, 2, 3, 4}) {
        for (const int b : {1, 2, 3, 4}) {
            if (b % a == 0) le.add(a, b);
        }
    }
    CHECK(le.isReflexive());
    CHECK(le.isAntisymmetric());
    CHECK(le.isTransitive());
    CHECK(le.isPartialOrder());
    CHECK_FALSE(le.isSymmetric());
    CHECK_FALSE(le.isEquivalence());
    CHECK(le.isStrictOrder() == false);   // reflexive, not strict

    // strict < on the same set: irreflexive asymmetric transitive strict order
    Relation<int> lt(std::vector<int>{1, 2, 3, 4});
    for (const int a : {1, 2, 3, 4}) {
        for (const int b : {1, 2, 3, 4}) {
            if (a < b) lt.add(a, b);
        }
    }
    CHECK(lt.isIrreflexive());
    CHECK(lt.isAsymmetric());
    CHECK(lt.isTransitive());
    CHECK(lt.isStrictOrder());
    CHECK_FALSE(lt.isReflexive());
}

TEST_CASE("Relation: symmetric + reflexive + transitive equivalence") {
    Relation<int> r(std::vector<int>{1, 2, 3, 4, 5});
    r.add(1, 2); r.add(2, 1);
    r.add(3, 4); r.add(4, 3);
    const auto closed = r.equivalenceClosure();
    CHECK(closed.isEquivalence());
    const auto classes = closed.equivalenceClasses();
    CHECK_EQ(classes.size(), 3u);   // {1,2}, {3,4}, {5}
}

TEST_CASE("Relation: Warshall closure agrees with brute-force oracle") {
    std::mt19937_64 rng(424242);
    for (int trial = 0; trial < 20; ++trial) {
        const std::vector<int> elems{0, 1, 2, 3, 4, 5};
        Relation<int> r(elems);
        std::set<std::pair<int, int>> oracle;
        const int pairs = static_cast<int>(rng() % 14);
        for (int i = 0; i < pairs; ++i) {
            const int a = static_cast<int>(rng() % 6);
            const int b = static_cast<int>(rng() % 6);
            r.add(a, b);
            oracle.insert({a, b});
        }
        const auto fast = r.transitiveClosure();
        const auto slow = bruteClosure(oracle, elems);
        CHECK_EQ(fast.pairCount(), slow.size());
        for (const auto& [a, b] : slow) {
            CHECK(fast.contains(a, b));
        }
        for (const auto& [a, b] : oracle) {
            CHECK(fast.contains(a, b));
        }
    }
}

TEST_CASE("Relation: equivalence classes match DisjointSet classes") {
    const std::vector<std::string> ids{"a", "b", "c", "d", "e"};
    Relation<std::string> r(ids);
    r.add("a", "b"); r.add("b", "a");
    r.add("c", "d"); r.add("d", "c");
    const auto closed = r.equivalenceClosure();
    const auto relationClasses = closed.equivalenceClasses();

    verity::ds::DisjointSet dsu(ids.size());
    const auto indexOf = [&ids](const std::string& s) {
        return static_cast<std::size_t>(std::find(ids.begin(), ids.end(), s) - ids.begin());
    };
    dsu.unionSets(indexOf("a"), indexOf("b"));
    dsu.unionSets(indexOf("c"), indexOf("d"));
    std::vector<std::vector<std::string>> dsuClasses(5);
    for (std::size_t i = 0; i < ids.size(); ++i) dsuClasses[dsu.find(i)].push_back(ids[i]);
    std::vector<std::vector<std::string>> dsuSorted;
    for (auto& c : dsuClasses) {
        if (!c.empty()) { std::sort(c.begin(), c.end()); dsuSorted.push_back(c); }
    }

    REQUIRE_EQ(relationClasses.size(), dsuSorted.size());
    for (const auto& cls : relationClasses) {
        std::vector<std::string> sortedCls = cls;
        std::sort(sortedCls.begin(), sortedCls.end());
        CHECK(std::find(dsuSorted.begin(), dsuSorted.end(), sortedCls) != dsuSorted.end());
    }
}

TEST_CASE("Relation: Hasse reduction removes exactly the transitive edges") {
    Relation<int> r(std::vector<int>{1, 2, 3, 4});
    for (const int a : {1, 2, 3, 4}) {
        for (const int b : {1, 2, 3, 4}) {
            if (a < b) r.add(a, b);
        }
    }
    const auto hasse = r.hasseDiagram();
    // cover edges only: 1<2, 2<3, 3<4 (1<3, 1<4, 2<4 are implied)
    CHECK(hasse.contains(1, 2));
    CHECK(hasse.contains(2, 3));
    CHECK(hasse.contains(3, 4));
    CHECK_FALSE(hasse.contains(1, 3));
    CHECK_FALSE(hasse.contains(1, 4));
    CHECK_FALSE(hasse.contains(2, 4));
}

TEST_CASE("Relation: bounds, lub/glb, lattice, incomparables") {
    Relation<int> r(std::vector<int>{1, 2, 3, 4, 6, 12});
    for (const int a : {1, 2, 3, 4, 6, 12}) {
        for (const int b : {1, 2, 3, 4, 6, 12}) {
            if (b % a == 0) r.add(a, b);
        }
    }
    CHECK_EQ(r.leastUpperBound(2, 3), 6);
    CHECK_EQ(r.greatestLowerBound(4, 6), 2);
    CHECK_EQ(r.leastUpperBound(1, 1), 1);
    CHECK(r.isLattice());
    const auto lower = r.lowerBounds(4, 6);
    CHECK(std::find(lower.begin(), lower.end(), 1) != lower.end());
    CHECK(std::find(lower.begin(), lower.end(), 2) != lower.end());

    // incomparable pairs on the diamond poset {1,2,3,6}: 2 and 3 incomparable
    Relation<int> d(std::vector<int>{1, 2, 3, 6});
    for (const int a : {1, 2, 3, 6}) {
        for (const int b : {1, 2, 3, 6}) {
            if (b % a == 0) d.add(a, b);
        }
    }
    const auto inc = d.incomparablePairs();
    REQUIRE_EQ(inc.size(), 1u);
    const bool pairMatches =
        (inc[0].first == 2 && inc[0].second == 3) || (inc[0].first == 3 && inc[0].second == 2);
    CHECK(pairMatches);

    const auto minimal = d.minimalElements();
    const auto maximal = d.maximalElements();
    CHECK_EQ(minimal.size(), 1u);
    CHECK_EQ(minimal[0], 1);
    CHECK_EQ(maximal.size(), 1u);
    CHECK_EQ(maximal[0], 6);
}

TEST_CASE("Relation: inverse, composition, union, intersection, complement") {
    Relation<int> r(std::vector<int>{1, 2, 3});
    r.add(1, 2);
    r.add(2, 3);
    const auto inv = r.inverse();
    CHECK(inv.contains(2, 1));
    CHECK(inv.contains(3, 2));
    const auto comp = r.compose(r);
    CHECK(comp.contains(1, 3));
    CHECK_FALSE(comp.contains(1, 2));
    const auto u = r.unionWith(inv);
    CHECK(u.contains(1, 2));
    CHECK(u.contains(2, 1));
    const auto i = r.intersectWith(inv);
    CHECK_EQ(i.pairCount(), 0u);
    const auto c = r.complement();
    CHECK(c.contains(1, 1));
    CHECK_FALSE(c.contains(1, 2));
}

TEST_CASE("Relation: partition checks") {
    const std::vector<std::vector<int>> good{{1, 2}, {3}, {4, 5}};
    CHECK(Relation<int>::partitionIsDisjoint(good));
    CHECK(Relation<int>::partitionCovers(good, std::vector<int>{1, 2, 3, 4, 5}));
    const std::vector<std::vector<int>> overlapping{{1, 2}, {2, 3}};
    CHECK_FALSE(Relation<int>::partitionIsDisjoint(overlapping));
    const std::vector<std::vector<int>> incomplete{{1}, {2}};
    CHECK_FALSE(Relation<int>::partitionCovers(incomplete, std::vector<int>{1, 2, 3}));
}

TEST_CASE("Relation: error paths") {
    Relation<int> r(std::vector<int>{1, 2});
    CHECK_THROWS_AS((void)r.contains(1, 9), verity::util::VerityError);
    // hasseDiagram on a non-order (symmetric cycle) must throw
    Relation<int> s(std::vector<int>{1, 2, 3});
    s.add(1, 2); s.add(2, 1);
    CHECK_THROWS_AS((void)s.hasseDiagram(), verity::util::VerityError);
    // equivalenceClasses on a non-equivalence must throw
    CHECK_THROWS_AS((void)r.equivalenceClasses(), verity::util::VerityError);
}
