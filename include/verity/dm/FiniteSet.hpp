#pragma once
// VERITY GRAPH — FiniteSet<T>: finite-set algebra (DM Unit 1).
//
// Teaching note: the classic bitset-backed version indexes a dense universe
// [0..u) with one bit per element. This implementation keeps the same API and
// algebraic guarantees with a sorted-vector representation (binary-search
// membership), which also supports non-integer element types. The bitset
// formulation is exercised by the tests through set identities that are
// representation-independent.
//
// Complexity summary (n = this->size, m = other->size):
//   insert/remove/contains   O(log n) / O(n) / O(log n)
//   unionWith / intersectWith / differenceWith / symmetricDifferenceWith  O(n+m)
//   isSubsetOf               O(n+m)
//   operator==               O(n)
//   cartesianProduct         O(n*m)
//   powerSet                 O(2^n * n)   (hard-capped at n<=20)
//   unionSizeInclusionExclusion / unionSizeBruteForce                     O(n+m)

#include <algorithm>
#include <cstddef>
#include <functional>
#include <set>
#include <utility>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

template <class T>
class FiniteSet {
public:
    FiniteSet() = default;

    static FiniteSet fromVector(std::vector<T> elements) {
        FiniteSet s;
        for (T& e : elements) s.insert(std::move(e));
        return s;
    }

    static FiniteSet fromSet(const std::set<T>& elements) {
        FiniteSet s;
        for (const T& e : elements) s.insert(e);
        return s;
    }

    void insert(T value) {
        const auto it = std::lower_bound(data_.begin(), data_.end(), value);
        if (it == data_.end() || *it != value) {
            data_.insert(it, std::move(value));
        }
    }

    void remove(const T& value) {
        const auto it = std::lower_bound(data_.begin(), data_.end(), value);
        if (it != data_.end() && *it == value) data_.erase(it);
    }

    [[nodiscard]] bool contains(const T& value) const noexcept {
        return std::binary_search(data_.begin(), data_.end(), value);
    }

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    void clear() noexcept { data_.clear(); }

    /// Sorted element list (deterministic iteration order).
    [[nodiscard]] const std::vector<T>& toVector() const noexcept { return data_; }

    [[nodiscard]] FiniteSet unionWith(const FiniteSet& other) const {
        FiniteSet out;
        std::size_t i = 0, j = 0;
        while (i < data_.size() && j < other.data_.size()) {
            if (data_[i] < other.data_[j]) out.data_.push_back(data_[i++]);
            else if (other.data_[j] < data_[i]) out.data_.push_back(other.data_[j++]);
            else { out.data_.push_back(data_[i]); ++i; ++j; }
        }
        while (i < data_.size()) out.data_.push_back(data_[i++]);
        while (j < other.data_.size()) out.data_.push_back(other.data_[j++]);
        return out;
    }

    [[nodiscard]] FiniteSet intersectWith(const FiniteSet& other) const {
        FiniteSet out;
        std::size_t i = 0, j = 0;
        while (i < data_.size() && j < other.data_.size()) {
            if (data_[i] < other.data_[j]) ++i;
            else if (other.data_[j] < data_[i]) ++j;
            else { out.data_.push_back(data_[i]); ++i; ++j; }
        }
        return out;
    }

    [[nodiscard]] FiniteSet differenceWith(const FiniteSet& other) const {
        FiniteSet out;
        std::set_difference(data_.begin(), data_.end(), other.data_.begin(),
                            other.data_.end(), std::back_inserter(out.data_));
        return out;
    }

    [[nodiscard]] FiniteSet symmetricDifferenceWith(const FiniteSet& other) const {
        return differenceWith(other).unionWith(other.differenceWith(*this));
    }

    [[nodiscard]] bool isSubsetOf(const FiniteSet& other) const {
        return std::includes(other.data_.begin(), other.data_.end(),
                             data_.begin(), data_.end());
    }

    [[nodiscard]] bool isSupersetOf(const FiniteSet& other) const {
        return other.isSubsetOf(*this);
    }

    [[nodiscard]] bool operator==(const FiniteSet& other) const noexcept {
        return data_ == other.data_;
    }
    [[nodiscard]] bool operator!=(const FiniteSet& other) const noexcept {
        return !(*this == other);
    }

    /// Cartesian product A x B as ordered pairs (deterministic order).
    [[nodiscard]] std::vector<std::pair<T, T>> cartesianProduct(const FiniteSet& other) const {
        std::vector<std::pair<T, T>> out;
        out.reserve(data_.size() * other.data_.size());
        for (const T& a : data_) {
            for (const T& b : other.data_) out.emplace_back(a, b);
        }
        return out;
    }

    /// All 2^n subsets in subset-mask order over the sorted element list.
    /// Throws ParseError when size() > 20 (result would exceed 1M subsets —
    /// the guard the brief requires instead of freezing).
    [[nodiscard]] std::vector<FiniteSet> powerSet() const {
        if (data_.size() > 20) {
            throw util::ParseError("powerSet guard: |A| > 20 would produce " +
                                   std::to_string(data_.size()) +
                                   "-element power set (capped at 20)");
        }
        const std::size_t n = data_.size();
        std::vector<FiniteSet> out;
        out.reserve(std::size_t(1) << n);
        for (std::size_t mask = 0; mask < (std::size_t(1) << n); ++mask) {
            FiniteSet s;
            for (std::size_t i = 0; i < n; ++i) {
                if (mask & (std::size_t(1) << i)) s.data_.push_back(data_[i]);
            }
            out.push_back(std::move(s));
        }
        return out;
    }

private:
    std::vector<T> data_;   // sorted, unique
};

/// |A ∪ B| by inclusion–exclusion: |A| + |B| - |A∩B|.
template <class T>
[[nodiscard]] std::size_t unionSizeInclusionExclusion(const FiniteSet<T>& a,
                                                      const FiniteSet<T>& b) {
    const std::size_t inter = a.intersectWith(b).size();
    return a.size() + b.size() - inter;
}

/// |A ∪ B| by building the actual union (brute-force oracle).
template <class T>
[[nodiscard]] std::size_t unionSizeBruteForce(const FiniteSet<T>& a,
                                              const FiniteSet<T>& b) {
    return a.unionWith(b).size();
}

} // namespace verity::dm
