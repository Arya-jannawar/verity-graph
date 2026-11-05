#pragma once
// VERITY GRAPH — ds/AVLTree: self-balancing BST with range queries (Phase 2).
// TimelineBuilder uses it as the date index (between() returns events whose
// date serials fall inside an interval).
//
// Keys must support operator< (and be copyable). Duplicate keys are rejected
// (insert returns false) — timeline serials are made unique by the caller.
//
// Complexity summary (n = node count):
//   insert / remove / contains   O(log n)   (balance restored by rotations)
//   between(a, b)                O(log n + k)
//   inOrder                      O(n)
//   size / height                O(1) / O(1)

#include <cstddef>
#include <functional>
#include <cstdlib>
#include <memory>
#include <vector>

namespace verity::ds {

template <class T>
class AVLTree {
    struct Node {
        T key;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        int height = 1;
        explicit Node(T k) : key(std::move(k)) {}
    };

public:
    /// Inserts `key`; returns false when the key already exists.
    bool insert(const T& key) {
        return insertNode(root_, key);
    }

    /// Removes `key`; returns false when absent.
    bool remove(const T& key) {
        const bool found = contains(key);
        if (found) root_ = removeNode(std::move(root_), key);
        return found;
    }

    [[nodiscard]] bool contains(const T& key) const {
        const Node* node = root_.get();
        while (node != nullptr) {
            if (key < node->key) node = node->left.get();
            else if (node->key < key) node = node->right.get();
            else return true;
        }
        return false;
    }

    /// Keys x with a <= x <= b (inclusive), ascending.
    [[nodiscard]] std::vector<T> between(const T& a, const T& b) const {
        std::vector<T> out;
        betweenRange(root_.get(), a, b, out);
        return out;
    }

    [[nodiscard]] std::vector<T> inOrder() const {
        std::vector<T> out;
        inOrderWalk(root_.get(), out);
        return out;
    }

    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    [[nodiscard]] int height() const noexcept { return nodeHeight(root_.get()); }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    /// Structural AVL invariant: |h(left)-h(right)| <= 1 everywhere (test hook).
    [[nodiscard]] bool isBalanced() const { return checkBalanced(root_.get()).first; }
    /// BST invariant with strict ordering (test hook).
    [[nodiscard]] bool isOrdered() const {
        const std::vector<T> ordered = inOrder();
        for (std::size_t i = 1; i < ordered.size(); ++i) {
            if (!(ordered[i - 1] < ordered[i])) return false;
        }
        return true;
    }

private:
    [[nodiscard]] static int nodeHeight(const Node* node) noexcept {
        return node == nullptr ? 0 : node->height;
    }

    static void updateHeight(Node* node) noexcept {
        node->height = 1 + (nodeHeight(node->left.get()) > nodeHeight(node->right.get())
                                ? nodeHeight(node->left.get())
                                : nodeHeight(node->right.get()));
    }

    [[nodiscard]] static int balanceFactor(const Node* node) noexcept {
        return node == nullptr ? 0 : nodeHeight(node->left.get()) - nodeHeight(node->right.get());
    }

    [[nodiscard]] static std::unique_ptr<Node> rotateRight(std::unique_ptr<Node> y) {
        auto x = std::move(y->left);
        y->left = std::move(x->right);
        x->right = std::move(y);
        updateHeight(x->right.get());
        updateHeight(x.get());
        return x;
    }

    [[nodiscard]] static std::unique_ptr<Node> rotateLeft(std::unique_ptr<Node> x) {
        auto y = std::move(x->right);
        x->right = std::move(y->left);
        y->left = std::move(x);
        updateHeight(y->left.get());
        updateHeight(y.get());
        return y;
    }

    [[nodiscard]] static std::unique_ptr<Node> rebalance(std::unique_ptr<Node> node) {
        updateHeight(node.get());
        const int bf = balanceFactor(node.get());
        if (bf > 1) {
            if (balanceFactor(node->left.get()) < 0) {
                node->left = rotateLeft(std::move(node->left));
            }
            return rotateRight(std::move(node));
        }
        if (bf < -1) {
            if (balanceFactor(node->right.get()) > 0) {
                node->right = rotateRight(std::move(node->right));
            }
            return rotateLeft(std::move(node));
        }
        return node;
    }

    bool insertNode(std::unique_ptr<Node>& node, const T& key) {
        if (node == nullptr) {
            node = std::make_unique<Node>(key);
            ++size_;
            return true;
        }
        bool inserted;
        if (key < node->key) {
            inserted = insertNode(node->left, key);
        } else if (node->key < key) {
            inserted = insertNode(node->right, key);
        } else {
            return false;   // duplicate
        }
        if (inserted) {
            // rebalance along the entire recursion path (heights changed below)
            node = rebalance(std::move(node));
        }
        return inserted;
    }

    [[nodiscard]] std::unique_ptr<Node> removeNode(std::unique_ptr<Node> node, const T& key) {
        if (node == nullptr) return nullptr;
        if (key < node->key) {
            node->left = removeNode(std::move(node->left), key);
        } else if (node->key < key) {
            node->right = removeNode(std::move(node->right), key);
        } else {
            if (node->left == nullptr || node->right == nullptr) {
                --size_;
                return std::move(node->left != nullptr ? node->left : node->right);
            }
            // two children: replace with in-order successor
            Node* succ = node->right.get();
            while (succ->left != nullptr) succ = succ->left.get();
            node->key = succ->key;
            node->right = removeNode(std::move(node->right), succ->key);
        }
        return rebalance(std::move(node));
    }

    static void betweenRange(const Node* node, const T& a, const T& b, std::vector<T>& out) {
        if (node == nullptr) return;
        if (a < node->key) betweenRange(node->left.get(), a, b, out);
        if (!(node->key < a) && !(b < node->key)) out.push_back(node->key);
        if (node->key < b) betweenRange(node->right.get(), a, b, out);
    }

    static void inOrderWalk(const Node* node, std::vector<T>& out) {
        if (node == nullptr) return;
        inOrderWalk(node->left.get(), out);
        out.push_back(node->key);
        inOrderWalk(node->right.get(), out);
    }

    [[nodiscard]] static std::pair<bool, int> checkBalanced(const Node* node) {
        if (node == nullptr) return {true, 0};
        const auto [leftOk, leftH] = checkBalanced(node->left.get());
        if (!leftOk) return {false, 0};
        const auto [rightOk, rightH] = checkBalanced(node->right.get());
        if (!rightOk) return {false, 0};
        return {std::abs(leftH - rightH) <= 1, 1 + (leftH > rightH ? leftH : rightH)};
    }

    std::unique_ptr<Node> root_;
    std::size_t size_ = 0;
};

} // namespace verity::ds
