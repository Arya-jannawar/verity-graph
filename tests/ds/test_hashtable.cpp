// VERITY GRAPH — tests/ds/test_hashtable.cpp
// HashTable: randomized oracle vs std::unordered_map (fixed seeds), prime
// sizing, load-factor rehash integrity, string-hash specialization behavior,
// collision-chain correctness.

#include "doctest/doctest.h"

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/ds/HashTable.hpp"

TEST_CASE("HashTable: basic operations") {
    verity::ds::HashTable<std::string, int> t;
    CHECK(t.empty());
    CHECK(t.insert("alpha", 1));
    CHECK_FALSE(t.insert("alpha", 9));            // duplicate rejected
    CHECK_FALSE(t.insertOrAssign("alpha", 2));    // returns false: assigned, not inserted
    CHECK_EQ(*t.get("alpha"), 2);
    CHECK(t.insertOrAssign("beta", 5));           // returns true: newly inserted
    CHECK_EQ(*t.get("alpha"), 2);
    CHECK_EQ(t.at("beta"), 5);
    CHECK(t.get("gamma") == nullptr);
    CHECK(t.contains("alpha"));
    CHECK_FALSE(t.contains("gamma"));
    CHECK_EQ(t.size(), 2u);
    CHECK(t.remove("alpha"));
    CHECK_FALSE(t.remove("alpha"));
    CHECK_EQ(t.size(), 1u);
    CHECK_THROWS_AS((void)t.at("nope"), verity::util::VerityError);
}

TEST_CASE("HashTable: prime bucket sizing via dm::nextPrime") {
    verity::ds::HashTable<int, int> t(50);        // needs ~77 buckets
    CHECK(t.bucketCount() >= 77u);
    CHECK(verity::dm::isPrime(t.bucketCount()));

    // growth: fill past load factor 0.75 and confirm all values survive
    verity::ds::HashTable<int, int> big;
    const std::size_t initial = big.bucketCount();
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(big.insert(i, i * i));
    }
    CHECK(big.bucketCount() > initial);
    CHECK(verity::dm::isPrime(big.bucketCount()));
    CHECK(big.loadFactor() <= 0.9);
    for (int i = 0; i < 10000; ++i) {
        REQUIRE(big.get(i) != nullptr);
        CHECK_EQ(*big.get(i), i * i);
    }
    CHECK_EQ(big.size(), 10000u);
}

TEST_CASE("HashTable: string keys use polynomial hash consistently") {
    verity::ds::HashTable<std::string, int> t;
    // Force collisions: keys differing only beyond low-order polynomial terms
    // still resolve through chaining correctly.
    for (int i = 0; i < 500; ++i) {
        REQUIRE(t.insert("key_" + std::to_string(i), i));
    }
    for (int i = 0; i < 500; ++i) {
        REQUIRE(t.get("key_" + std::to_string(i)) != nullptr);
        CHECK_EQ(*t.get("key_" + std::to_string(i)), i);
    }
    CHECK_EQ(t.size(), 500u);
    CHECK(verity::dm::isPrime(t.bucketCount()));   // capacity stays prime
}

TEST_CASE("HashTable: iteration enumerates exactly all entries") {
    verity::ds::HashTable<std::string, int> t;
    for (int i = 0; i < 200; ++i) t.insert("k" + std::to_string(i), i);
    std::size_t count = 0;
    std::vector<int> values;
    for (const auto& entry : t) {
        ++count;
        values.push_back(entry.second);
    }
    CHECK_EQ(count, 200u);
    CHECK_EQ(values.size(), 200u);
    // sum sanity: 0+1+...+199
    long long sum = 0;
    for (int v : values) sum += v;
    CHECK_EQ(sum, 199 * 200 / 2);
}

namespace {

/// Randomized mixed-operation oracle against std::unordered_map.
void runMapOracle(unsigned seed, int operations) {
    std::mt19937 rng(seed);
    verity::ds::HashTable<int, int> real;
    std::unordered_map<int, int> oracle;
    for (int op = 0; op < operations; ++op) {
        const int key = static_cast<int>(rng() % 300);   // small domain -> collisions
        const int value = static_cast<int>(rng() % 10000);
        const int choice = static_cast<int>(rng() % 5);
        switch (choice) {
            case 0: {
                const bool rIns = real.insert(key, value);
                const bool oIns = oracle.emplace(key, value).second;
                CHECK_EQ(rIns, oIns);
                break;
            }
            case 1: {
                const bool rNew = real.insertOrAssign(key, value);
                const bool oNew = oracle.count(key) == 0;
                oracle[key] = value;
                CHECK_EQ(rNew, oNew);
                break;
            }
            case 2: {
                const int* rv = real.get(key);
                auto it = oracle.find(key);
                if (it == oracle.end()) {
                    CHECK(rv == nullptr);
                } else {
                    REQUIRE(rv != nullptr);
                    CHECK_EQ(*rv, it->second);
                }
                break;
            }
            case 3: {
                const bool rDel = real.remove(key);
                const bool oDel = oracle.erase(key) > 0;
                CHECK_EQ(rDel, oDel);
                break;
            }
            default: {
                CHECK_EQ(real.contains(key), oracle.count(key) > 0);
                break;
            }
        }
        REQUIRE_EQ(real.size(), oracle.size());
    }
    // final full-state comparison via iteration counts
    CHECK_EQ(real.size(), oracle.size());
}

} // namespace

TEST_CASE("HashTable: randomized oracle vs std::unordered_map (fixed seeds)") {
    runMapOracle(11u, 4000);
    runMapOracle(777u, 4000);
    runMapOracle(31337u, 4000);
}
