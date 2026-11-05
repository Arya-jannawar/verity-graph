// VERITY GRAPH — tests/dm/test_sets.cpp
// FiniteSet: randomized oracle vs std::set, algebra laws, power set (bounded),
// inclusion-exclusion vs brute force.

#include "doctest/doctest.h"

#include <algorithm>
#include <random>
#include <set>
#include <vector>

#include "verity/dm/FiniteSet.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::FiniteSet;
using verity::dm::unionSizeBruteForce;
using verity::dm::unionSizeInclusionExclusion;

namespace {

FiniteSet<int> fromStd(const std::set<int>& s) {
    return FiniteSet<int>::fromSet(s);
}

} // namespace

TEST_CASE("FiniteSet: insert/remove/contains basics") {
    FiniteSet<int> s;
    CHECK(s.empty());
    s.insert(3);
    s.insert(1);
    s.insert(2);
    s.insert(3);                        // idempotent
    CHECK_EQ(s.size(), 3u);
    CHECK(s.contains(1));
    CHECK(s.contains(2));
    CHECK(s.contains(3));
    CHECK_FALSE(s.contains(4));
    CHECK_EQ(s.toVector(), std::vector<int>({1, 2, 3}));   // sorted order
    s.remove(2);
    CHECK_FALSE(s.contains(2));
    CHECK_EQ(s.size(), 2u);
    s.clear();
    CHECK(s.empty());
}

TEST_CASE("FiniteSet: algebra laws over fixed sets") {
    const FiniteSet<int> A = FiniteSet<int>::fromVector({1, 2, 3, 4});
    const FiniteSet<int> B = FiniteSet<int>::fromVector({3, 4, 5});
    const FiniteSet<int> U = FiniteSet<int>::fromVector({1, 2, 3, 4, 5, 6});

    CHECK_EQ(A.unionWith(B), FiniteSet<int>::fromVector({1, 2, 3, 4, 5}));
    CHECK_EQ(A.intersectWith(B), FiniteSet<int>::fromVector({3, 4}));
    CHECK_EQ(A.differenceWith(B), FiniteSet<int>::fromVector({1, 2}));
    CHECK_EQ(B.differenceWith(A), FiniteSet<int>::fromVector({5}));
    CHECK_EQ(A.symmetricDifferenceWith(B), FiniteSet<int>::fromVector({1, 2, 5}));

    // De Morgan over complement-in-U: !(A∪B) = !A ∩ !B
    const FiniteSet<int> notAB = U.differenceWith(A.unionWith(B));
    const FiniteSet<int> notA_and_notB = U.differenceWith(A).intersectWith(U.differenceWith(B));
    CHECK_EQ(notAB, notA_and_notB);

    // involution and absorption
    CHECK_EQ(A.unionWith(A.intersectWith(B)), A);                 // absorption
    CHECK_EQ(A.intersectWith(A.unionWith(B)), A);                 // absorption
    CHECK_EQ(A.differenceWith(A), FiniteSet<int>());              // A \ A = {}

    CHECK(A.isSubsetOf(U));
    CHECK(U.isSupersetOf(A));
    CHECK_FALSE(A.isSubsetOf(B));
    CHECK_EQ(A, FiniteSet<int>::fromVector({4, 2, 1, 3}));        // order-independent equality
}

TEST_CASE("FiniteSet: cartesian product") {
    const FiniteSet<int> A = FiniteSet<int>::fromVector({1, 2});
    const FiniteSet<int> B = FiniteSet<int>::fromVector({10, 20, 30});
    const auto product = A.cartesianProduct(B);
    REQUIRE_EQ(product.size(), 6u);
    CHECK_EQ(product[0], std::make_pair(1, 10));
    CHECK_EQ(product[5], std::make_pair(2, 30));
    const FiniteSet<int> empty;
    CHECK(A.cartesianProduct(empty).empty());
}

TEST_CASE("FiniteSet: power set sizes and content for n<=12, guard above 20") {
    for (std::size_t n = 0; n <= 12; ++n) {
        std::vector<int> elems;
        for (std::size_t i = 1; i <= n; ++i) elems.push_back(static_cast<int>(i));
        const auto ps = FiniteSet<int>::fromVector(elems).powerSet();
        CHECK_EQ(ps.size(), (std::size_t{1} << n));   // |P(A)| = 2^n
    }
    // content check for a 4-element set vs brute-force std::set-of-sets
    const auto base = FiniteSet<int>::fromVector({1, 2, 3, 4});
    const auto ps = base.powerSet();
    std::set<std::set<int>> expected;
    for (int mask = 0; mask < 16; ++mask) {
        std::set<int> s;
        for (int i = 0; i < 4; ++i) {
            if (mask & (1 << i)) s.insert(i + 1);
        }
        expected.insert(s);
    }
    std::set<std::set<int>> got;
    for (const auto& subset : ps) {
        got.insert(std::set<int>(subset.toVector().begin(), subset.toVector().end()));
    }
    CHECK_EQ(got, expected);

    // guard: 21 elements must throw instead of freezing
    std::vector<int> big;
    for (int i = 0; i < 21; ++i) big.push_back(i);
    CHECK_THROWS_AS((void)FiniteSet<int>::fromVector(big).powerSet(), verity::util::VerityError);
    // 20 elements is allowed (1,048,576 subsets) but only count-check the small side
    std::vector<int> edge;
    for (int i = 0; i < 20; ++i) edge.push_back(i);
    CHECK_EQ(FiniteSet<int>::fromVector(edge).powerSet().size(), std::size_t{1} << 20);
}

TEST_CASE("FiniteSet: randomized oracle vs std::set (fixed seeds)") {
    std::mt19937 rng(99);
    FiniteSet<int> real;
    std::set<int> oracle;
    for (int op = 0; op < 3000; ++op) {
        const int value = static_cast<int>(rng() % 40);   // small universe
        const int choice = static_cast<int>(rng() % 3);
        if (choice == 0) {
            real.insert(value);
            oracle.insert(value);
        } else if (choice == 1) {
            real.remove(value);
            oracle.erase(value);
        } else {
            CHECK_EQ(real.contains(value), oracle.count(value) > 0);
        }
        REQUIRE_EQ(real.size(), oracle.size());
    }
    std::vector<int> oVec(oracle.begin(), oracle.end());
    CHECK_EQ(real.toVector(), oVec);
}

TEST_CASE("FiniteSet: set-operation oracle on random pairs + IE identity") {
    std::mt19937 rng(1234);
    for (int trial = 0; trial < 300; ++trial) {
        std::set<int> sa, sb;
        for (int i = 0; i < 20; ++i) sa.insert(static_cast<int>(rng() % 25));
        for (int i = 0; i < 20; ++i) sb.insert(static_cast<int>(rng() % 25));
        const FiniteSet<int> A = fromStd(sa);
        const FiniteSet<int> B = fromStd(sb);

        std::set<int> uni, inter;
        std::set_union(sa.begin(), sa.end(), sb.begin(), sb.end(),
                       std::inserter(uni, uni.begin()));
        std::set_intersection(sa.begin(), sa.end(), sb.begin(), sb.end(),
                              std::inserter(inter, inter.begin()));
        CHECK_EQ(A.unionWith(B), fromStd(uni));
        CHECK_EQ(A.intersectWith(B), fromStd(inter));

        std::set<int> sym;
        std::set_symmetric_difference(sa.begin(), sa.end(), sb.begin(), sb.end(),
                                      std::inserter(sym, sym.begin()));
        CHECK_EQ(A.symmetricDifferenceWith(B), fromStd(sym));

        CHECK_EQ(unionSizeInclusionExclusion(A, B), unionSizeBruteForce(A, B));
        CHECK_EQ(unionSizeInclusionExclusion(A, B), uni.size());
    }
}
