// VERITY GRAPH — tests/ds/test_ds_core.cpp
// LinkedList / Stack / Queue: randomized oracle tests against std::vector and
// std::list with fixed seeds, Rule-of-Five deep-copy checks, exception paths.

#include "doctest/doctest.h"

#include <list>
#include <random>
#include <string>
#include <vector>

#include "verity/ds/LinkedList.hpp"
#include "verity/ds/Queue.hpp"
#include "verity/ds/Stack.hpp"
#include "verity/util/VerityError.hpp"

namespace {

/// A type that counts live instances so copy/move/destruction is observable.
struct Traced {
    static int live;
    int value;
    explicit Traced(int v) : value(v) { ++live; }
    Traced(const Traced& o) : value(o.value) { ++live; }
    Traced(Traced&& o) noexcept : value(o.value) { ++live; }
    Traced& operator=(const Traced&) = default;
    Traced& operator=(Traced&&) = default;
    ~Traced() { --live; }
    bool operator==(const Traced& o) const { return value == o.value; }
};
int Traced::live = 0;

} // namespace

TEST_CASE("LinkedList: basic sequence operations") {
    verity::ds::LinkedList<int> l;
    CHECK(l.empty());
    CHECK_EQ(l.size(), 0u);

    l.push_back(2);
    l.push_front(1);
    l.push_back(3);
    CHECK_EQ(l.size(), 3u);
    CHECK_EQ(l.front(), 1);
    CHECK_EQ(l.back(), 3);

    std::vector<int> seen;
    for (int v : l) seen.push_back(v);
    CHECK_EQ(seen, std::vector<int>{1, 2, 3});

    l.pop_front();
    l.pop_back();
    CHECK_EQ(l.size(), 1u);
    CHECK_EQ(l.front(), 2);

    SUBCASE("pop on empty throws") {
        verity::ds::LinkedList<int> e;
        CHECK_THROWS_AS(e.pop_front(), verity::util::VerityError);
        CHECK_THROWS_AS(e.pop_back(), verity::util::VerityError);
    }
}

TEST_CASE("LinkedList: removeFirstMatching") {
    verity::ds::LinkedList<int> l{1, 4, 3, 4, 5};
    CHECK(l.removeFirstMatching([](int v) { return v == 4; }));
    std::vector<int> seen;
    for (int v : l) seen.push_back(v);
    CHECK_EQ(seen, std::vector<int>({1, 3, 4, 5}));  // first 4 removed, tail intact

    // removing the tail fixes back()
    CHECK(l.removeFirstMatching([](int v) { return v == 5; }));
    CHECK_EQ(l.back(), 4);

    // removing the head fixes front()
    CHECK(l.removeFirstMatching([](int v) { return v == 1; }));
    CHECK_EQ(l.front(), 3);
    CHECK_EQ(l.size(), 2u);

    CHECK_FALSE(l.removeFirstMatching([](int) { return false; }));
}

TEST_CASE("LinkedList: Rule of Five deep copy") {
    verity::ds::LinkedList<std::string> a{"x", "y", "z"};
    verity::ds::LinkedList<std::string> b(a);          // copy ctor
    b.push_back("w");
    CHECK_EQ(a.size(), 3u);                            // original unchanged
    CHECK_EQ(b.size(), 4u);

    verity::ds::LinkedList<std::string> c;
    c = a;                                             // copy assignment
    c.pop_front();
    CHECK_EQ(a.size(), 3u);
    CHECK_EQ(c.size(), 2u);

    verity::ds::LinkedList<std::string> d(std::move(a));  // move ctor
    CHECK_EQ(d.size(), 3u);
    CHECK_EQ(a.size(), 0u);                            // moved-from is empty
    CHECK(a.empty());

    verity::ds::LinkedList<std::string> e;
    e = std::move(d);                                  // move assignment
    CHECK_EQ(e.size(), 3u);
    CHECK(d.empty());

    // self-assignment (copy and move) must be safe
    verity::ds::LinkedList<int> s{7, 8};
    const auto& alias = s;
    s = alias;                                         // self copy-assign via alias
    CHECK_EQ(s.size(), 2u);
    // self move-assign via a reference-laundry helper (a literal
    // `s = std::move(s)` is flagged -Wself-move by clang)
    auto&& moveRef = [](verity::ds::LinkedList<int>& x) -> verity::ds::LinkedList<int>&& {
        return std::move(x);
    };
    s = moveRef(s);
    CHECK_EQ(s.size(), 2u);
}

TEST_CASE("LinkedList: no leaks across random churn (Traced counter)") {
    const int before = Traced::live;
    {
        verity::ds::LinkedList<Traced> l;
        for (int i = 0; i < 500; ++i) l.push_back(Traced(i));
        verity::ds::LinkedList<Traced> copy = l;
        verity::ds::LinkedList<Traced> moved = std::move(l);
        while (!moved.empty()) moved.pop_front();
        copy.clear();
    }
    CHECK_EQ(Traced::live, before);
}

TEST_CASE("Stack: LIFO semantics and exceptions") {
    verity::ds::Stack<int> s;
    CHECK_THROWS_AS(s.pop(), verity::util::VerityError);
    for (int i = 0; i < 100; ++i) s.push(i);
    CHECK_EQ(s.size(), 100u);
    CHECK_EQ(s.top(), 99);
    for (int i = 99; i >= 0; --i) {
        CHECK_EQ(s.pop(), i);
    }
    CHECK(s.empty());
}

TEST_CASE("Queue: FIFO semantics and exceptions") {
    verity::ds::Queue<int> q;
    CHECK_THROWS_AS(q.dequeue(), verity::util::VerityError);
    for (int i = 0; i < 100; ++i) q.enqueue(i);
    CHECK_EQ(q.size(), 100u);
    CHECK_EQ(q.front(), 0);
    CHECK_EQ(q.back(), 99);
    for (int i = 0; i < 100; ++i) {
        CHECK_EQ(q.dequeue(), i);
    }
    CHECK(q.empty());
}

namespace {

/// Oracle model driving both LinkedList and std::list with identical ops.
void runListOracle(unsigned seed, int operations) {
    std::mt19937 rng(seed);
    verity::ds::LinkedList<int> real;
    std::list<int> oracle;
    for (int op = 0; op < operations; ++op) {
        const int choice = static_cast<int>(rng() % 6);
        const int value = static_cast<int>(rng() % 1000);
        switch (choice) {
            case 0: real.push_back(value); oracle.push_back(value); break;
            case 1: real.push_front(value); oracle.push_front(value); break;
            case 2:
                if (!oracle.empty()) {
                    real.pop_front();
                    oracle.pop_front();
                }
                break;
            case 3: {
                // remove first matching value (may not exist in either)
                const int target = static_cast<int>(rng() % 1000);
                const bool rHad = real.removeFirstMatching(
                    [&](int v) { return v == target; });
                const auto it = std::find(oracle.begin(), oracle.end(), target);
                const bool oHad = it != oracle.end();
                if (oHad) oracle.erase(it);
                CHECK_EQ(rHad, oHad);
                break;
            }
            case 4:
                if (!oracle.empty()) {
                    CHECK_EQ(real.front(), oracle.front());
                }
                break;
            default:
                if (!oracle.empty()) {
                    CHECK_EQ(real.back(), oracle.back());
                }
                break;
        }
        REQUIRE_EQ(real.size(), oracle.size());
        auto rit = real.begin();
        auto oit = oracle.begin();
        while (oit != oracle.end()) {
            REQUIRE_EQ(*rit, *oit);
            ++rit;
            ++oit;
        }
    }
}

} // namespace

TEST_CASE("LinkedList: randomized oracle vs std::list (fixed seeds)") {
    for (unsigned seed : {1u, 42u, 1234u, 98765u}) {
        runListOracle(seed, 400);
    }
}

TEST_CASE("Stack/Queue: randomized oracle vs std::vector/std::list") {
    std::mt19937 rng(2024);
    verity::ds::Stack<int> stack;
    std::vector<int> sOracle;
    verity::ds::Queue<int> queue;
    std::list<int> qOracle;
    for (int op = 0; op < 3000; ++op) {
        const int choice = static_cast<int>(rng() % 4);
        const int value = static_cast<int>(rng() % 100);
        if (choice <= 1) {
            stack.push(value);
            sOracle.push_back(value);
            queue.enqueue(value);
            qOracle.push_back(value);
        } else {
            if (!sOracle.empty()) {
                CHECK_EQ(stack.pop(), sOracle.back());
                sOracle.pop_back();
            }
            if (!qOracle.empty()) {
                CHECK_EQ(queue.dequeue(), qOracle.front());
                qOracle.pop_front();
            }
        }
        REQUIRE_EQ(stack.size(), sOracle.size());
        REQUIRE_EQ(queue.size(), qOracle.size());
    }
}
