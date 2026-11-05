#pragma once
// VERITY GRAPH — dm/Combinatorics: permutations, enumeration, linear
// extensions, pigeonhole (DM Unit 2, Phase 2).
//
// Complexity summary:
//   nextPermutation         O(n) amortized (lexicographic successor)
//   combinationsRecursive   O(C(n,k)·k)
//   combinationsIterative   O(C(n,k)·k)
//   permutationsRecursive   O(n!·n)      (guarded: n ≤ 8)
//   countLinearExtensions   O(n! · n)    (guarded: n ≤ 9)
//   pigeonholeHolds         O(1)

#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Lexicographic next permutation (own implementation, mirrors
/// std::next_permutation semantics: transforms `seq` into the next
/// lexicographic permutation; returns false and wraps to sorted-ascending
/// when seq is the last permutation). O(n).
template <class T>
bool nextPermutation(std::vector<T>& seq) {
    if (seq.size() < 2) return false;
    const std::size_t n = seq.size();
    std::size_t i = n - 1;
    while (i > 0 && !(seq[i - 1] < seq[i])) --i;
    if (i == 0) {
        std::reverse(seq.begin(), seq.end());
        return false;
    }
    const std::size_t pivot = i - 1;
    std::size_t j = n - 1;
    while (!(seq[pivot] < seq[j])) --j;
    std::swap(seq[pivot], seq[j]);
    std::reverse(seq.begin() + static_cast<std::ptrdiff_t>(pivot + 1), seq.end());
    return true;
}

namespace detail {

/// Enumerate k-combinations of `items` by recursive inclusion/exclusion.
template <class T>
void combinationsRec(const std::vector<T>& items, std::size_t k, std::size_t start,
                     std::vector<T>& current, std::vector<std::vector<T>>& out) {
    if (current.size() == k) {
        out.push_back(current);
        return;
    }
    for (std::size_t i = start; i < items.size(); ++i) {
        current.push_back(items[i]);
        combinationsRec(items, k, i + 1, current, out);
        current.pop_back();
    }
}

/// Enumerate k-combinations by the standard iterative index-increment scheme.
template <class T>
void combinationsIter(const std::vector<T>& items, std::size_t k,
                      std::vector<std::vector<T>>& out) {
    const std::size_t n = items.size();
    if (k > n) return;
    std::vector<std::size_t> idx(k);
    for (std::size_t i = 0; i < k; ++i) idx[i] = i;
    while (true) {
        std::vector<T> combo;
        combo.reserve(k);
        for (std::size_t i : idx) combo.push_back(items[i]);
        out.push_back(std::move(combo));
        // advance to next combination of indices: find the rightmost slot
        // that can still be incremented (>= is the saturation test; a strict
        // == here would let out-of-range vectors slip through)
        std::size_t pos = k;
        while (pos > 0 && idx[pos - 1] >= n - (k - pos)) --pos;
        if (pos == 0) break;
        ++idx[pos - 1];
        for (std::size_t i = pos; i < k; ++i) idx[i] = idx[i - 1] + 1;
    }
}

template <class T>
void permutationsRec(std::vector<T>& items, std::size_t start,
                     std::vector<std::vector<T>>& out) {
    if (start == items.size()) {
        out.push_back(items);
        return;
    }
    for (std::size_t i = start; i < items.size(); ++i) {
        std::swap(items[start], items[i]);
        permutationsRec(items, start + 1, out);
        std::swap(items[start], items[i]);
    }
}

} // namespace detail

/// All k-combinations of `items` (recursive inclusion/exclusion tree).
template <class T>
[[nodiscard]] std::vector<std::vector<T>> combinationsRecursive(const std::vector<T>& items,
                                                                std::size_t k) {
    std::vector<std::vector<T>> out;
    std::vector<T> current;
    detail::combinationsRec(items, k, 0, current, out);
    return out;
}

/// All k-combinations of `items` (iterative index scheme).
template <class T>
[[nodiscard]] std::vector<std::vector<T>> combinationsIterative(const std::vector<T>& items,
                                                                std::size_t k) {
    std::vector<std::vector<T>> out;
    detail::combinationsIter(items, k, out);
    return out;
}

/// All n! permutations (recursive Heap-style swap enumeration). Guarded:
/// throws for n > 8 (40320 permutations is the cap).
template <class T>
[[nodiscard]] std::vector<std::vector<T>> permutationsRecursive(const std::vector<T>& items) {
    if (items.size() > 8) {
        throw util::ParseError("permutationsRecursive guard: n > 8 would produce " +
                               std::to_string(items.size()) + "! permutations");
    }
    std::vector<std::vector<T>> out;
    std::vector<T> work = items;
    detail::permutationsRec(work, 0, out);
    return out;
}

/// Count the linear extensions of a poset given as an adjacency matrix
/// (edge[i][j] means i must precede j) by exhaustive backtracking.
/// Guarded: throws for n > 9 (362880 partial orders).
[[nodiscard]] inline std::size_t countLinearExtensions(const std::vector<std::vector<bool>>& before) {
    const std::size_t n = before.size();
    if (n > 9) {
        throw util::ParseError("countLinearExtensions guard: n > 9 would exceed 9! branches");
    }
    std::size_t count = 0;
    std::vector<bool> used(n, false);
    std::vector<std::size_t> placed;
    placed.reserve(n);

    std::function<void()> backtrack = [&]() {
        if (placed.size() == n) {
            ++count;
            return;
        }
        for (std::size_t candidate = 0; candidate < n; ++candidate) {
            if (used[candidate]) continue;
            // every predecessor must already be placed
            bool ready = true;
            for (std::size_t pred = 0; pred < n && ready; ++pred) {
                if (before[pred][candidate] && !used[pred]) ready = false;
            }
            if (!ready) continue;
            used[candidate] = true;
            placed.push_back(candidate);
            backtrack();
            placed.pop_back();
            used[candidate] = false;
        }
    };
    backtrack();
    return count;
}

/// Pigeonhole principle check: with `items` items in `boxes` boxes and
/// items > boxes, at least one box holds ≥ 2 items — the checker validates
/// the premise and reports that the conclusion holds.
[[nodiscard]] inline bool pigeonholeHolds(std::size_t items, std::size_t boxes) {
    return items > boxes;
}

} // namespace verity::dm
