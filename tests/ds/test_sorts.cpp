// VERITY GRAPH — tests/ds/test_sorts.cpp
// mergeSort/quickSort vs std::sort oracle (randomized, fixed seeds), binary
// search vs std::lower_bound, Levenshtein known values + DP table.

#include "doctest/doctest.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "verity/ds/Levenshtein.hpp"
#include "verity/ds/SortAlgo.hpp"

namespace {

std::vector<int> randomVector(std::mt19937& rng, std::size_t size, int lo, int hi) {
    std::vector<int> out;
    out.reserve(size);
    for (std::size_t i = 0; i < size; ++i) {
        out.push_back(lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1)));
    }
    return out;
}

} // namespace

TEST_CASE("mergeSort: randomized oracle vs std::sort") {
    std::mt19937 rng(1001);
    for (int trial = 0; trial < 30; ++trial) {
        const auto data = randomVector(rng, 1 + rng() % 200, -50, 50);
        const auto sorted = verity::ds::mergeSort(data);
        auto expected = data;
        std::sort(expected.begin(), expected.end());
        CHECK_EQ(sorted, expected);
        // input untouched
        const auto reSorted = verity::ds::mergeSort(data);
        CHECK_EQ(sorted, reSorted);
    }
}

TEST_CASE("mergeSort is stable (pairs sorted by first only)") {
    const std::vector<std::pair<int, char>> in{{2, 'a'}, {1, 'b'}, {2, 'c'}, {1, 'd'}};
    const auto out = verity::ds::mergeSort(in, [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    const std::vector<std::pair<int, char>> expected{{1, 'b'}, {1, 'd'}, {2, 'a'}, {2, 'c'}};
    CHECK_EQ(out, expected);
}

TEST_CASE("quickSort: randomized oracle vs std::sort") {
    std::mt19937 rng(2002);
    for (int trial = 0; trial < 30; ++trial) {
        const auto data = randomVector(rng, 1 + rng() % 200, -50, 50);
        const auto sorted = verity::ds::quickSort(data);
        auto expected = data;
        std::sort(expected.begin(), expected.end());
        CHECK_EQ(sorted, expected);
    }
    // empty and single
    CHECK(verity::ds::quickSort(std::vector<int>{}).empty());
    CHECK_EQ(verity::ds::quickSort(std::vector<int>{7}), std::vector<int>{7});
}

TEST_CASE("binarySearch: oracle vs std::lower_bound (present and absent)") {
    std::mt19937 rng(3003);
    const auto data = randomVector(rng, 300, 0, 200);
    auto sorted = data;
    std::sort(sorted.begin(), sorted.end());
    for (int probe = -5; probe <= 205; ++probe) {
        const std::size_t idx = verity::ds::binarySearch(sorted, probe);
        const auto ref = std::lower_bound(sorted.begin(), sorted.end(), probe);
        const bool refFound = ref != sorted.end() && *ref == probe;
        if (refFound) {
            REQUIRE_NE(idx, static_cast<std::size_t>(-1));
            CHECK_EQ(sorted[idx], probe);
        } else {
            CHECK_EQ(idx, static_cast<std::size_t>(-1));
        }
    }
}

TEST_CASE("Levenshtein: known values") {
    CHECK_EQ(verity::ds::levenshtein("", ""), 0u);
    CHECK_EQ(verity::ds::levenshtein("abc", "abc"), 0u);
    CHECK_EQ(verity::ds::levenshtein("", "abc"), 3u);
    CHECK_EQ(verity::ds::levenshtein("abc", ""), 3u);
    CHECK_EQ(verity::ds::levenshtein("kitten", "sitting"), 3u);
    CHECK_EQ(verity::ds::levenshtein("smith", "smyth"), 1u);
    CHECK_EQ(verity::ds::levenshtein("john", "jon"), 1u);
    CHECK_EQ(verity::ds::levenshtein("flaw", "lawn"), 2u);
}

TEST_CASE("Levenshtein: DP table structure and symmetry-ish checks") {
    std::vector<std::vector<std::size_t>> table;
    const std::size_t d = verity::ds::levenshtein("sitting", "kitten", &table);
    CHECK_EQ(d, 3u);
    REQUIRE_EQ(table.size(), 8u);         // (7+1) rows
    REQUIRE_EQ(table[0].size(), 7u);      // (6+1) columns
    CHECK_EQ(table[0][0], 0u);
    for (std::size_t i = 0; i <= 7; ++i) CHECK_EQ(table[i][0], i);
    for (std::size_t j = 0; j <= 6; ++j) CHECK_EQ(table[0][j], j);
    CHECK_EQ(table[7][6], d);             // final cell
    // triangle inequality spot check
    CHECK(verity::ds::levenshtein("smith", "smyth") <=
          verity::ds::levenshtein("smith", "john") + verity::ds::levenshtein("john", "smyth"));
}
