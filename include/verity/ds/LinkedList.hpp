#pragma once
// VERITY GRAPH — LinkedList<T>: handwritten singly linked list with iterators.
//
// Used internally by Stack, Queue and HashTable (separate chaining), i.e. it
// is a real engine-path structure, not a demo artifact.
//
// Complexity summary:
//   push_front      O(1)
//   push_back       O(1)         (tail pointer)
//   pop_front       O(1)
//   pop_back        O(n)         (singly linked: need predecessor)
//   front/back      O(1)
//   size/empty      O(1)
//   clear           O(n)
//   removeFirstMatching(pred)  O(n)
//   copy ctor/assign O(n)        (deep)
//   move ctor/assign O(1)
//   iteration       O(n)

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <utility>

#include "verity/util/VerityError.hpp"

namespace verity::ds {

template <class T>
class LinkedList {
    struct Node {
        T value;
        Node* next;
    };

public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        explicit iterator(Node* node = nullptr) noexcept : node_(node) {}
        reference operator*() const noexcept { return node_->value; }
        pointer operator->() const noexcept { return &node_->value; }
        iterator& operator++() noexcept {
            node_ = node_->next;
            return *this;
        }
        iterator operator++(int) noexcept {
            iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        [[nodiscard]] bool operator==(const iterator& o) const noexcept { return node_ == o.node_; }
        [[nodiscard]] bool operator!=(const iterator& o) const noexcept { return node_ != o.node_; }

    private:
        friend class LinkedList;
        Node* node_;
    };

    class const_iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        explicit const_iterator(const Node* node = nullptr) noexcept : node_(node) {}
        const_iterator(const iterator& it) noexcept : node_(it.node_) {}
        reference operator*() const noexcept { return node_->value; }
        pointer operator->() const noexcept { return &node_->value; }
        const_iterator& operator++() noexcept {
            node_ = node_->next;
            return *this;
        }
        const_iterator operator++(int) noexcept {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
        }
        [[nodiscard]] bool operator==(const const_iterator& o) const noexcept { return node_ == o.node_; }
        [[nodiscard]] bool operator!=(const const_iterator& o) const noexcept { return node_ != o.node_; }

    private:
        friend class LinkedList;
        const Node* node_;
    };

    LinkedList() noexcept = default;

    LinkedList(std::initializer_list<T> init) {
        for (const T& v : init) push_back(v);
    }

    LinkedList(const LinkedList& other) {
        for (const T& v : other) push_back(v);
    }

    LinkedList& operator=(const LinkedList& other) {
        if (this != &other) {
            LinkedList tmp(other);
            *this = std::move(tmp);
        }
        return *this;
    }

    LinkedList(LinkedList&& other) noexcept
        : head_(other.head_), tail_(other.tail_), size_(other.size_) {
        other.head_ = other.tail_ = nullptr;
        other.size_ = 0;
    }

    LinkedList& operator=(LinkedList&& other) noexcept {
        if (this != &other) {
            clear();
            head_ = other.head_;
            tail_ = other.tail_;
            size_ = other.size_;
            other.head_ = other.tail_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    ~LinkedList() { clear(); }

    void push_front(T value) {
        head_ = new Node{std::move(value), head_};
        if (tail_ == nullptr) tail_ = head_;
        ++size_;
    }

    void push_back(T value) {
        Node* n = new Node{std::move(value), nullptr};
        if (tail_ == nullptr) {
            head_ = tail_ = n;
        } else {
            tail_->next = n;
            tail_ = n;
        }
        ++size_;
    }

    void pop_front() {
        if (head_ == nullptr) {
            throw util::VerityError("LinkedList::pop_front on empty list");
        }
        Node* dead = head_;
        head_ = head_->next;
        if (tail_ == dead) tail_ = nullptr;
        delete dead;
        --size_;
    }

    void pop_back() {
        if (tail_ == nullptr) {
            throw util::VerityError("LinkedList::pop_back on empty list");
        }
        if (head_ == tail_) {
            delete head_;
            head_ = tail_ = nullptr;
            size_ = 0;
            return;
        }
        Node* prev = head_;
        while (prev->next != tail_) prev = prev->next;
        prev->next = nullptr;
        delete tail_;
        tail_ = prev;
        --size_;
    }

    [[nodiscard]] T& front() noexcept { return head_->value; }
    [[nodiscard]] const T& front() const noexcept { return head_->value; }
    [[nodiscard]] T& back() noexcept { return tail_->value; }
    [[nodiscard]] const T& back() const noexcept { return tail_->value; }

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }

    void clear() noexcept {
        Node* n = head_;
        while (n != nullptr) {
            Node* dead = n;
            n = n->next;
            delete dead;
        }
        head_ = tail_ = nullptr;
        size_ = 0;
    }

    /// Removes the first element satisfying `pred`. Returns true if removed.
    bool removeFirstMatching(const std::function<bool(const T&)>& pred) {
        Node** link = &head_;
        while (*link != nullptr) {
            if (pred((*link)->value)) {
                Node* dead = *link;
                *link = dead->next;
                if (dead == tail_) tail_ = nullptr;  // fix below if needed
                // recompute tail if we removed the last node
                if (tail_ == nullptr) {
                    Node* n = head_;
                    if (n == nullptr) {
                        tail_ = nullptr;
                    } else {
                        while (n->next != nullptr) n = n->next;
                        tail_ = n;
                    }
                }
                delete dead;
                --size_;
                return true;
            }
            link = &(*link)->next;
        }
        return false;
    }

    iterator begin() noexcept { return iterator(head_); }
    iterator end() noexcept { return iterator(nullptr); }
    const_iterator begin() const noexcept { return const_iterator(head_); }
    const_iterator end() const noexcept { return const_iterator(nullptr); }
    const_iterator cbegin() const noexcept { return const_iterator(head_); }
    const_iterator cend() const noexcept { return const_iterator(nullptr); }

private:
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
};

} // namespace verity::ds
