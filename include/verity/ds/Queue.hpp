#pragma once
// VERITY GRAPH — Queue<T>: FIFO adapter composed on the handwritten LinkedList.
// Used by BFS (real engine path).
//
// Complexity summary:
//   enqueue  O(1)      dequeue O(1) (throws on empty)
//   front/back O(1)    size O(1)    empty O(1)    clear O(n)

#include "verity/ds/LinkedList.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

template <class T>
class Queue {
public:
    void enqueue(T value) { list_.push_back(std::move(value)); }

    /// Removes and returns the front element; throws on empty.
    T dequeue() {
        if (list_.empty()) {
            throw util::VerityError("Queue::dequeue on empty queue");
        }
        T value = std::move(list_.front());
        list_.pop_front();
        return value;
    }

    [[nodiscard]] T& front() noexcept { return list_.front(); }
    [[nodiscard]] const T& front() const noexcept { return list_.front(); }
    [[nodiscard]] T& back() noexcept { return list_.back(); }
    [[nodiscard]] const T& back() const noexcept { return list_.back(); }
    [[nodiscard]] bool empty() const noexcept { return list_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return list_.size(); }
    void clear() noexcept { list_.clear(); }

private:
    LinkedList<T> list_;
};

} // namespace verity::ds
