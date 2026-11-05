#pragma once
// VERITY GRAPH — ds/Heap: binary min-heap / max-heap with decreaseKey and a
// top-K helper (Phase 2). Phase 3 uses MinHeap for Dijkstra and the top-K
// evidence ranker.
//
// MinHeap: std::vector storage, siftUp/siftDown. decreaseKey takes the index
// of an element (caller-tracked) and its new value. MaxHeap<T> = MinHeap with
// a reversed comparator.
//
// Complexity summary (n = heap size):
//   push / pop   O(log n)
//   peek         O(1)
//   decreaseKey  O(log n)
//   topK         O(n log k)   (bounded heap)

#include <cstddef>
#include <functional>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::ds {

/// Binary min-heap over values of type T with comparator Less (strict).
template <class T, class Less = std::less<T>>
class MinHeap {
public:
    MinHeap() = default;
    explicit MinHeap(Less less) : less_(std::move(less)) {}

    void push(T value) {
        data_.push_back(std::move(value));
        siftUp(data_.size() - 1);
    }

    /// Removes and returns the smallest element; throws on empty.
    T pop() {
        if (data_.empty()) throw util::VerityError("MinHeap::pop on empty heap");
        T top = std::move(data_.front());
        if (data_.size() > 1) {
            data_.front() = std::move(data_.back());
            data_.pop_back();
            siftDown(0);
        } else {
            data_.pop_back();
        }
        return top;
    }

    [[nodiscard]] const T& peek() const {
        if (data_.empty()) throw util::VerityError("MinHeap::peek on empty heap");
        return data_.front();
    }

    /// Replaces data_[index] with a SMALLER value and restores the heap.
    void decreaseKey(std::size_t index, T newValue) {
        if (index >= data_.size()) throw util::VerityError("decreaseKey index out of range");
        if (less_(data_[index], newValue)) {
            throw util::VerityError("decreaseKey requires a smaller-or-equal new value");
        }
        data_[index] = std::move(newValue);
        siftUp(index);
    }

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }

private:
    void siftUp(std::size_t i) {
        while (i > 0) {
            const std::size_t parent = (i - 1) / 2;
            if (!less_(data_[i], data_[parent])) break;
            std::swap(data_[i], data_[parent]);
            i = parent;
        }
    }

    void siftDown(std::size_t i) {
        const std::size_t n = data_.size();
        while (true) {
            const std::size_t l = 2 * i + 1;
            const std::size_t r = 2 * i + 2;
            std::size_t smallest = i;
            if (l < n && less_(data_[l], data_[smallest])) smallest = l;
            if (r < n && less_(data_[r], data_[smallest])) smallest = r;
            if (smallest == i) break;
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }

    std::vector<T> data_;
    Less less_{};
};

/// Max-heap: MinHeap with a reversed comparator.
template <class T, class Less = std::less<T>>
class MaxHeap {
public:
    struct Reversed {
        Less less;
        template <class U>
        [[nodiscard]] bool operator()(const U& a, const U& b) const {
            return less(b, a);
        }
    };

    void push(T value) { heap_.push(std::move(value)); }
    T pop() { return heap_.pop(); }
    [[nodiscard]] const T& peek() const { return heap_.peek(); }
    [[nodiscard]] bool empty() const noexcept { return heap_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return heap_.size(); }

private:
    MinHeap<T, Reversed> heap_;
};

/// The k largest items (by `greater` ordering) of `items`, best first.
/// O(n log k) via a bounded min-heap. k == 0 returns empty.
template <class T, class Greater = std::greater<T>>
[[nodiscard]] std::vector<T> topK(const std::vector<T>& items, std::size_t k, Greater greater = {}) {
    std::vector<T> out;
    if (k == 0) return out;
    MinHeap<T, std::function<bool(const T&, const T&)>> heap(
        [greater](const T& a, const T& b) { return greater(b, a); });
    // heap holds the k best seen so far; its top is the weakest of them
    for (const T& item : items) {
        if (heap.size() < k) {
            heap.push(item);
        } else if (greater(item, heap.peek())) {
            (void)heap.pop();
            heap.push(item);
        }
    }
    out.reserve(heap.size());
    while (!heap.empty()) out.push_back(heap.pop());
    // popped ascending (weakest first) -> reverse for best-first
    return std::vector<T>(out.rbegin(), out.rend());
}

} // namespace verity::ds
