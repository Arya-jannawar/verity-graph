// VERITY GRAPH — tests/ds/test_ds2.cpp
// Phase-2 structures: Trie (case-insensitive, prefix, delete), AVL (balance,
// range query), DisjointSet (vs brute force), NaryTree (traversals), Heap
// (top-K vs std::sort oracle).

#include "doctest/doctest.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "verity/ds/AVLTree.hpp"
#include "verity/ds/DisjointSet.hpp"
#include "verity/ds/Heap.hpp"
#include "verity/ds/NaryTree.hpp"
#include "verity/ds/Trie.hpp"
#include "verity/util/VerityError.hpp"

TEST_CASE("Trie: insert/search/prefix/delete, case-insensitive") {
    verity::ds::Trie trie;
    CHECK(trie.insert("John Smith"));
    CHECK_FALSE(trie.insert("John Smith"));   // duplicate
    CHECK(trie.insert("Maria Chen"));
    CHECK(trie.search("john SMITH"));          // case-insensitive
    CHECK(trie.startsWith("john s"));
    CHECK_FALSE(trie.search("maria"));
    CHECK(trie.search("maria chen"));
    CHECK(trie.remove("Maria Chen"));
    CHECK_FALSE(trie.search("maria chen"));
    CHECK_FALSE(trie.remove("Maria Chen"));    // already gone
    CHECK_FALSE(trie.remove("nonexistent"));
}

TEST_CASE("Trie: wordsWithPrefix returns originals, sorted") {
    verity::ds::Trie trie;
    for (const char* w : {"Smith, John", "John Smith", "J. Smith", "Mr. Smith", "Maria Chen"}) {
        trie.insert(w);
    }
    const auto jWords = trie.wordsWithPrefix("j");
    REQUIRE_EQ(jWords.size(), 2u);   // J. Smith, John Smith
    CHECK_EQ(jWords[0], "J. Smith");
    CHECK_EQ(jWords[1], "John Smith");
    // punctuation-transparent prefix: "smit" hits the Smith, John path
    const auto smithWords = trie.wordsWithPrefix("smith");
    REQUIRE_EQ(smithWords.size(), 1u);
    CHECK_EQ(smithWords[0], "Smith, John");
    CHECK(trie.wordsWithPrefix("zzz").empty());
}

TEST_CASE("Trie: randomized oracle vs std::set (fixed seed)") {
    std::mt19937 rng(2024);
    verity::ds::Trie trie;
    std::set<std::string> oracle;
    const std::string alphabet = "abcd";
    for (int op = 0; op < 2000; ++op) {
        std::string word;
        const int len = static_cast<int>(rng() % 5) + 1;
        for (int i = 0; i < len; ++i) word += alphabet[rng() % alphabet.size()];
        const int choice = static_cast<int>(rng() % 3);
        if (choice == 0) {
            CHECK_EQ(trie.insert(word), oracle.insert(word).second);
        } else if (choice == 1) {
            CHECK_EQ(trie.search(word), oracle.count(word) > 0);
        } else {
            CHECK_EQ(trie.remove(word), oracle.erase(word) > 0);
        }
        CHECK_EQ(trie.size(), oracle.size());
    }
    // prefix oracle on a subset
    std::vector<std::string> expected;
    for (const std::string& w : oracle) {
        if (w.rfind("ab", 0) == 0) expected.push_back(w);
    }
    std::sort(expected.begin(), expected.end());
    CHECK_EQ(trie.wordsWithPrefix("ab"), expected);
}

TEST_CASE("AVLTree: insert/remove/balance/range query") {
    verity::ds::AVLTree<int> tree;
    for (int i = 1; i <= 1000; ++i) tree.insert(i);   // ascending inserts (worst case for BST)
    CHECK_EQ(tree.size(), 1000u);
    CHECK_EQ(tree.height(), 10);      // log2(1000) ~ 10, AVL guarantees ~1.44 log
    CHECK(tree.isBalanced());
    CHECK(tree.isOrdered());

    const auto range = tree.between(300, 320);
    REQUIRE_EQ(range.size(), 21u);
    CHECK_EQ(range.front(), 300);
    CHECK_EQ(range.back(), 320);

    for (int i = 1; i <= 500; ++i) CHECK(tree.remove(i));
    CHECK_EQ(tree.size(), 500u);
    CHECK(tree.isBalanced());
    CHECK_FALSE(tree.remove(1));      // already removed
    CHECK_FALSE(tree.contains(1));
    CHECK(tree.contains(501));
}

TEST_CASE("AVLTree: randomized oracle vs std::set") {
    std::mt19937 rng(99);
    verity::ds::AVLTree<int> tree;
    std::set<int> oracle;
    for (int op = 0; op < 3000; ++op) {
        const int value = static_cast<int>(rng() % 300);
        const int choice = static_cast<int>(rng() % 3);
        if (choice == 0) CHECK_EQ(tree.insert(value), oracle.insert(value).second);
        else if (choice == 1) CHECK_EQ(tree.remove(value), oracle.erase(value) > 0);
        else CHECK_EQ(tree.contains(value), oracle.count(value) > 0);
        CHECK_EQ(tree.size(), oracle.size());
    }
    CHECK(tree.isOrdered());
    CHECK(tree.isBalanced());
    const auto inOrder = tree.inOrder();
    REQUIRE_EQ(inOrder.size(), oracle.size());
    std::size_t i = 0;
    for (const int v : oracle) CHECK_EQ(inOrder[i++], v);
    // range query oracle
    const auto range = tree.between(50, 100);
    std::vector<int> expected;
    for (const int v : oracle) {
        if (50 <= v && v <= 100) expected.push_back(v);
    }
    CHECK_EQ(range, expected);
}

TEST_CASE("DisjointSet: unions, path compression, component counting") {
    verity::ds::DisjointSet dsu(10);
    CHECK_EQ(dsu.setCount(), 10u);
    CHECK(dsu.unionSets(0, 1));
    CHECK_FALSE(dsu.unionSets(1, 0));       // already united
    CHECK(dsu.unionSets(2, 3));
    CHECK(dsu.unionSets(0, 2));             // merges {0,1} and {2,3}
    CHECK(dsu.connected(1, 3));
    CHECK_FALSE(dsu.connected(4, 5));
    CHECK_EQ(dsu.setCount(), 7u);
}

TEST_CASE("DisjointSet: randomized oracle vs brute-force connectivity") {
    std::mt19937 rng(555);
    verity::ds::DisjointSet dsu(50);
    std::vector<std::set<std::size_t>> components(50);
    for (std::size_t i = 0; i < 50; ++i) components[i] = {i};
    for (int op = 0; op < 400; ++op) {
        const std::size_t a = rng() % 50;
        const std::size_t b = rng() % 50;
        const bool merged = dsu.unionSets(a, b);
        // brute-force merge
        std::size_t compA = 0, compB = 0;
        for (std::size_t c = 0; c < components.size(); ++c) {
            if (components[c].count(a)) compA = c;
            if (components[c].count(b)) compB = c;
        }
        if (compA != compB) {
            CHECK(merged);
            for (const std::size_t x : components[compB]) components[compA].insert(x);
            components[compB].clear();
        } else {
            CHECK_FALSE(merged);
        }
        CHECK_EQ(dsu.connected(a, b), components[compA].count(b) > 0);
    }
    std::size_t nonEmpty = 0;
    for (const auto& c : components) {
        if (!c.empty()) ++nonEmpty;
    }
    CHECK_EQ(dsu.setCount(), nonEmpty);
}

TEST_CASE("NaryTree: traversals, depth, height, leaves") {
    verity::ds::NaryTree<std::string> tree("root");
    auto& a = tree.root().addChild("a");
    auto& b = tree.root().addChild("b");
    a.addChild("a1");
    a.addChild("a2");
    b.addChild("b1");
    CHECK_EQ(tree.size(), 6u);

    std::vector<std::string> pre, post;
    tree.preOrder([&](const std::string& v) { pre.push_back(v); });
    tree.postOrder([&](const std::string& v) { post.push_back(v); });
    // pre-order visits node before children
    CHECK_EQ(pre, std::vector<std::string>({"root", "a", "a1", "a2", "b", "b1"}));
    // post-order visits children before node
    CHECK_EQ(post, std::vector<std::string>({"a1", "a2", "a", "b1", "b", "root"}));
    // level order visits breadth-first (handwritten Queue)
    CHECK_EQ(tree.levelOrder(),
             std::vector<std::string>({"root", "a", "b", "a1", "a2", "b1"}));
    CHECK_EQ(tree.height(), 2u);
    CHECK_EQ(tree.leafCount(), 3u);   // a1, a2, b1
    CHECK_EQ(tree.depth(&a), 1u);
    CHECK_EQ(tree.depth(&tree.root()), 0u);
}

TEST_CASE("MinHeap/MaxHeap: ordering, decreaseKey, empty pop throws") {
    verity::ds::MinHeap<int> heap;
    CHECK_THROWS_AS(heap.pop(), verity::util::VerityError);
    for (const int v : {5, 3, 8, 1, 9, 2}) heap.push(v);
    CHECK_EQ(heap.peek(), 1);
    const std::vector<int> expected{1, 2, 3, 5, 8, 9};
    for (const int e : expected) CHECK_EQ(heap.pop(), e);

    verity::ds::MaxHeap<int> maxHeap;
    for (const int v : {5, 3, 8, 1}) maxHeap.push(v);
    CHECK_EQ(maxHeap.peek(), 8);
    CHECK_EQ(maxHeap.pop(), 8);
    CHECK_EQ(maxHeap.pop(), 5);

    verity::ds::MinHeap<int> dk;
    for (const int v : {10, 20, 30}) dk.push(v);
    dk.decreaseKey(2, 1);   // replace 30 with 1
    CHECK_EQ(dk.peek(), 1);
    CHECK_THROWS_AS(dk.decreaseKey(0, 99), verity::util::VerityError);   // larger value rejected
    CHECK_THROWS_AS(dk.decreaseKey(10, 1), verity::util::VerityError);   // bad index
}

TEST_CASE("topK: oracle vs std::sort on random data") {
    std::mt19937 rng(777);
    std::vector<int> data(200);
    std::iota(data.begin(), data.end(), 0);
    std::shuffle(data.begin(), data.end(), rng);
    for (const std::size_t k : {1u, 3u, 10u, 199u, 200u, 500u}) {
        const auto got = verity::ds::topK(data, k);
        std::vector<int> expected = data;
        std::sort(expected.begin(), expected.end(), std::greater<int>());
        if (expected.size() > k) expected.resize(k);
        CHECK_EQ(got, expected);
    }
    CHECK(verity::ds::topK(data, 0).empty());
}
