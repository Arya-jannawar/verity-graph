// VERITY GRAPH — tests/dm/test_combinatorics.cpp
// nextPermutation vs std::next_permutation oracle, recursive/iterative
// combinations agreement, permutation enumeration counts, linear extensions
// (chain/diamond/antichain + brute-force cross-check), pigeonhole.

#include "doctest/doctest.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <vector>

#include "verity/dm/Combinatorics.hpp"
#include "verity/dm/Counting.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::combinationsIterative;
using verity::dm::combinationsRecursive;
using verity::dm::countLinearExtensions;
using verity::dm::nextPermutation;
using verity::dm::permutationsRecursive;
using verity::dm::pigeonholeHolds;

TEST_CASE("nextPermutation matches std::next_permutation over all orderings") {
    const std::vector<int> base{1, 2, 3, 4};
    std::vector<int> mine = base;
    std::vector<int> ref = base;
    int steps = 1;
    CHECK_EQ(mine, ref);
    while (nextPermutation(mine)) {
        REQUIRE(std::next_permutation(ref.begin(), ref.end()));
        CHECK_EQ(mine, ref);
        ++steps;
    }
    // both must exhaust together (4! = 24 permutations), both wrapping to
    // ascending order on the final call
    CHECK_EQ(steps, 24);
    CHECK_FALSE(std::next_permutation(ref.begin(), ref.end()));
    CHECK_EQ(mine, ref);
}

TEST_CASE("nextPermutation: single and empty sequences") {
    std::vector<int> single{7};
    CHECK_FALSE(nextPermutation(single));
    std::vector<int> empty;
    CHECK_FALSE(nextPermutation(empty));
}

TEST_CASE("combinations: recursive and iterative agree; counts equal nCr") {
    const std::vector<int> items{1, 2, 3, 4, 5, 6};
    for (std::size_t k = 0; k <= 6; ++k) {
        const auto rec = combinationsRecursive(items, k);
        const auto iter = combinationsIterative(items, k);
        REQUIRE_EQ(rec.size(), iter.size());
        CHECK_EQ(rec.size(), verity::dm::nCr(6, static_cast<int>(k)));
        for (std::size_t i = 0; i < rec.size(); ++i) {
            CHECK_EQ(rec[i], iter[i]);
            CHECK(std::is_sorted(rec[i].begin(), rec[i].end()));
        }
    }
    // k > n yields empty
    CHECK(combinationsRecursive(std::vector<int>{1}, 2).empty());
}

TEST_CASE("permutationsRecursive: n! enumeration and guard") {
    const auto perms = permutationsRecursive(std::vector<int>{1, 2, 3});
    CHECK_EQ(perms.size(), 6u);
    std::set<std::vector<int>> unique(perms.begin(), perms.end());
    CHECK_EQ(unique.size(), 6u);
    CHECK_THROWS_AS((void)permutationsRecursive(std::vector<int>(9, 1)),
                    verity::util::VerityError);
}

TEST_CASE("countLinearExtensions: chain, diamond, antichain") {
    // total chain a<b<c<d: exactly one extension
    const std::vector<std::vector<bool>> chain{
        {false, true, false, false},
        {false, false, true, false},
        {false, false, false, true},
        {false, false, false, false}};
    CHECK_EQ(countLinearExtensions(chain), 1u);

    // diamond a<b, a<c, b<d, c<d: two extensions (b,c in either order)
    const std::vector<std::vector<bool>> diamond{
        {false, true, true, false},
        {false, false, false, true},
        {false, false, false, true},
        {false, false, false, false}};
    CHECK_EQ(countLinearExtensions(diamond), 2u);

    // antichain on 3 elements: 3! = 6 extensions
    const std::vector<std::vector<bool>> antichain(3, std::vector<bool>(3, false));
    CHECK_EQ(countLinearExtensions(antichain), 6u);

    // V poset a<b, a<c: exactly 2 extensions (b and c in either order after a)
    const std::vector<std::vector<bool>> vee{
        {false, true, true},
        {false, false, false},
        {false, false, false}};
    CHECK_EQ(countLinearExtensions(vee), 2u);

    CHECK_THROWS_AS((void)countLinearExtensions(std::vector<std::vector<bool>>(10, std::vector<bool>(10, false))),
                    verity::util::VerityError);
}

TEST_CASE("countLinearExtensions: brute-force oracle on random posets") {
    std::mt19937 rng(31415);
    for (int trial = 0; trial < 25; ++trial) {
        const std::size_t n = 4 + rng() % 2;   // 4..5 elements
        std::vector<std::vector<bool>> before(n, std::vector<bool>(n, false));
        // build a random acyclic relation via a random permutation: i before j
        // when perm[i] < perm[j] and a random coin fires
        std::vector<int> perm(n);
        std::iota(perm.begin(), perm.end(), 0);
        std::shuffle(perm.begin(), perm.end(), rng);
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                if (i != j && perm[i] < perm[j] && rng() % 3 == 0) before[i][j] = true;
            }
        }
        // brute force: count permutations respecting the order
        std::size_t expected = 0;
        std::vector<int> seq(n);
        std::iota(seq.begin(), seq.end(), 0);
        std::vector<int> idx(n);
        do {
            // seq gives an order of elements; check feasibility
            std::vector<int> position(n);
            for (std::size_t p = 0; p < n; ++p) position[seq[p]] = static_cast<int>(p);
            bool ok = true;
            for (std::size_t i = 0; i < n && ok; ++i) {
                for (std::size_t j = 0; j < n && ok; ++j) {
                    if (before[i][j] && !(position[i] < position[j])) ok = false;
                }
            }
            if (ok) ++expected;
        } while (std::next_permutation(seq.begin(), seq.end()));
        CHECK_EQ(countLinearExtensions(before), expected);
    }
}

TEST_CASE("pigeonhole: premise and conclusion") {
    CHECK(pigeonholeHolds(10, 9));   // 10 items in 9 boxes: a box holds >= 2
    CHECK(pigeonholeHolds(3, 2));
    CHECK_FALSE(pigeonholeHolds(3, 3));   // no forced collision
    CHECK_FALSE(pigeonholeHolds(0, 5));
}
