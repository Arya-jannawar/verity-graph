#pragma once
// VERITY GRAPH — util/FuncTemplates: function templates (OOP Unit 4).
//
//   countIf      — counts elements satisfying a predicate
//   intersectSorted — intersection of two SORTED sequences (merge walk)
//
// Complexity: countIf O(n); intersectSorted O(n + m).

#include <cstddef>
#include <vector>

namespace verity::util {

template <class It, class Pred>
[[nodiscard]] std::size_t countIf(It first, It last, Pred pred) {
    std::size_t count = 0;
    for (; first != last; ++first) {
        if (pred(*first)) ++count;
    }
    return count;
}

/// Intersection of two ascending-sorted sequences; duplicates preserved per
/// multiplicity (set semantics when inputs are duplicate-free).
template <class T, class Less = std::less<T>>
[[nodiscard]] std::vector<T> intersectSorted(const std::vector<T>& a, const std::vector<T>& b,
                                             Less less = {}) {
    std::vector<T> out;
    std::size_t i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        if (less(a[i], b[j])) {
            ++i;
        } else if (less(b[j], a[i])) {
            ++j;
        } else {
            out.push_back(a[i]);
            ++i;
            ++j;
        }
    }
    return out;
}

} // namespace verity::util
