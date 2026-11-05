#pragma once
// VERITY GRAPH — ds/SortAlgo: handwritten merge sort, quick sort and binary
// search (Phase 2). Oracle-tested against std::sort / std::lower_bound.
//
// Complexity summary (n = length):
//   mergeSort    O(n log n), stable, O(n) extra
//   quickSort    O(n log n) average, O(n^2) worst, O(log n) stack
//   binarySearch O(log n) on a sorted sequence

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace verity::ds {

/// Stable merge sort (returns a new sorted vector; input untouched).
template <class T, class Less = std::less<T>>
[[nodiscard]] std::vector<T> mergeSort(const std::vector<T>& items, Less less = {}) {
    if (items.size() <= 1) return items;
    std::vector<T> buffer(items.size());
    std::vector<T> work(items);

    std::function<void(std::size_t, std::size_t)> sortRange = [&](std::size_t lo, std::size_t hi) {
        if (hi - lo <= 1) return;
        const std::size_t mid = lo + (hi - lo) / 2;
        sortRange(lo, mid);
        sortRange(mid, hi);
        // merge into buffer, then copy back
        std::size_t i = lo, j = mid, k = lo;
        while (i < mid && j < hi) {
            if (less(work[j], work[i])) buffer[k++] = std::move(work[j++]);
            else buffer[k++] = std::move(work[i++]);
        }
        while (i < mid) buffer[k++] = std::move(work[i++]);
        while (j < hi) buffer[k++] = std::move(work[j++]);
        for (std::size_t p = lo; p < hi; ++p) work[p] = std::move(buffer[p]);
    };
    sortRange(0, items.size());
    return work;
}

/// Quick sort with middle pivot and three-way partition (deterministic).
/// Returns a new sorted vector; input untouched.
template <class T, class Less = std::less<T>>
[[nodiscard]] std::vector<T> quickSort(const std::vector<T>& items, Less less = {}) {
    std::vector<T> work(items);
    std::function<void(std::ptrdiff_t, std::ptrdiff_t)> sortRange = [&](std::ptrdiff_t lo,
                                                                        std::ptrdiff_t hi) {
        if (lo >= hi) return;
        const T pivot = work[static_cast<std::size_t>(lo + (hi - lo) / 2)];
        std::ptrdiff_t i = lo, j = hi;
        while (i <= j) {
            while (less(work[static_cast<std::size_t>(i)], pivot)) ++i;
            while (less(pivot, work[static_cast<std::size_t>(j)])) --j;
            if (i <= j) {
                std::swap(work[static_cast<std::size_t>(i)], work[static_cast<std::size_t>(j)]);
                ++i;
                --j;
            }
        }
        sortRange(lo, j);
        sortRange(i, hi);
    };
    if (!work.empty()) sortRange(0, static_cast<std::ptrdiff_t>(work.size()) - 1);
    return work;
}

/// Binary search on a sorted (ascending by `less`) vector: index of `value`
/// or SIZE_MAX when absent. When duplicates exist, returns any one match.
template <class T, class Less = std::less<T>>
[[nodiscard]] std::size_t binarySearch(const std::vector<T>& sorted, const T& value,
                                       Less less = {}) {
    std::size_t lo = 0, hi = sorted.size();
    while (lo < hi) {
        const std::size_t mid = lo + (hi - lo) / 2;
        if (less(sorted[mid], value)) {
            lo = mid + 1;
        } else if (less(value, sorted[mid])) {
            hi = mid;
        } else {
            return mid;
        }
    }
    return static_cast<std::size_t>(-1);
}

} // namespace verity::ds
