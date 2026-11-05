#pragma once
// VERITY GRAPH — Stack<T>: LIFO adapter composed on the handwritten LinkedList.
// (Composition, not inheritance: the list is an implementation detail.)
//
// Complexity summary:
//   push  O(1)        pop  O(1) (throws on empty)
//   top   O(1)        size O(1)     empty O(1)     clear O(n)

#include "verity/ds/LinkedList.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

template <class T>
class Stack {
public:
    void push(T value) { list_.push_front(std::move(value)); }

    /// Removes and returns the top element; throws on empty.
    T pop() {
        if (list_.empty()) {
            throw util::VerityError("Stack::pop on empty stack");
        }
        T value = std::move(list_.front());
        list_.pop_front();
        return value;
    }

    [[nodiscard]] T& top() noexcept { return list_.front(); }
    [[nodiscard]] const T& top() const noexcept { return list_.front(); }
    [[nodiscard]] bool empty() const noexcept { return list_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return list_.size(); }
    void clear() noexcept { list_.clear(); }

private:
    LinkedList<T> list_;
};

} // namespace verity::ds
