#pragma once
// VERITY GRAPH — ds/DisjointSet: union-find with path compression + union by
// rank (Phase 2). EntityResolver cross-checks its equivalence classes against
// the dm::Relation closure; undirected cycle detection uses it (Phase 2), as
// does Kruskal's MST (Phase 3).
//
// Complexity summary (α = inverse Ackermann):
//   find       O(α(n)) amortized
//   unionSets  O(α(n)) amortized
//   connected  O(α(n))

#include <cstddef>
#include <numeric>
#include <vector>

namespace verity::ds {

class DisjointSet {
public:
    explicit DisjointSet(std::size_t n) : parent_(n), rank_(n, 0), sets_(n) {
        std::iota(parent_.begin(), parent_.end(), std::size_t{0});
    }

    /// Representative root of x's set (path-compressed).
    std::size_t find(std::size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];   // path halving
            x = parent_[x];
        }
        return x;
    }

    /// Unions the sets containing a and b; returns false when already united.
    bool unionSets(std::size_t a, std::size_t b) {
        const std::size_t ra = find(a);
        const std::size_t rb = find(b);
        if (ra == rb) return false;
        if (rank_[ra] < rank_[rb]) {
            parent_[ra] = rb;
        } else if (rank_[ra] > rank_[rb]) {
            parent_[rb] = ra;
        } else {
            parent_[rb] = ra;
            ++rank_[ra];
        }
        --sets_;
        return true;
    }

    [[nodiscard]] bool connected(std::size_t a, std::size_t b) { return find(a) == find(b); }
    [[nodiscard]] std::size_t setCount() const noexcept { return sets_; }
    [[nodiscard]] std::size_t size() const noexcept { return parent_.size(); }

private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> rank_;
    std::size_t sets_;
};

} // namespace verity::ds
