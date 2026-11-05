#pragma once
// VERITY GRAPH — ds/NaryTree: general rooted tree (Phase 2). DocumentTree
// traversals and the Phase-4 provenance origin tree verification reuse the
// traversal patterns (pre/post/level order, depth, height, leaf count).
//
// Owned children (unique_ptr). Level order uses the handwritten ds::Queue.
//
// Complexity summary (n = nodes):
//   addChild                O(1)
//   preOrder/postOrder      O(n)
//   levelOrder              O(n)
//   depth(node path length) O(depth)
//   height / leaves         O(n)

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

#include "verity/ds/Queue.hpp"

namespace verity::ds {

template <class T>
class NaryTree {
public:
    struct Node {
        T value;
        std::vector<std::unique_ptr<Node>> children;
        Node* parent = nullptr;

        explicit Node(T v) : value(std::move(v)) {}

        Node& addChild(T childValue) {
            children.push_back(std::make_unique<Node>(std::move(childValue)));
            children.back()->parent = this;
            return *children.back();
        }

        [[nodiscard]] bool isLeaf() const noexcept { return children.empty(); }
    };

    explicit NaryTree(T rootValue) {
        root_ = std::make_unique<Node>(std::move(rootValue));
        count_ = 1;
    }

    [[nodiscard]] Node& root() noexcept { return *root_; }
    [[nodiscard]] const Node& root() const noexcept { return *root_; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    void preOrder(const std::function<void(const T&)>& visit) const {
        preWalk(root_.get(), visit);
    }

    void postOrder(const std::function<void(const T&)>& visit) const {
        postWalk(root_.get(), visit);
    }

    [[nodiscard]] std::vector<T> levelOrder() const {
        std::vector<T> out;
        Queue<const Node*> frontier;
        frontier.enqueue(root_.get());
        while (!frontier.empty()) {
            const Node* node = frontier.dequeue();
            out.push_back(node->value);
            for (const auto& child : node->children) frontier.enqueue(child.get());
        }
        return out;
    }

    /// Distance from root to `node` (root has depth 0).
    [[nodiscard]] std::size_t depth(const Node* node) const {
        std::size_t d = 0;
        for (const Node* p = node; p != root_.get() && p != nullptr; p = p->parent) ++d;
        return d;
    }

    /// Longest root-to-leaf edge count (single node has height 0).
    [[nodiscard]] std::size_t height() const { return heightWalk(root_.get()); }

    [[nodiscard]] std::size_t leafCount() const {
        std::size_t leaves = 0;
        countLeaves(root_.get(), leaves);
        return leaves;
    }

private:
    static void preWalk(const Node* node, const std::function<void(const T&)>& visit) {
        visit(node->value);
        for (const auto& child : node->children) preWalk(child.get(), visit);
    }

    static void postWalk(const Node* node, const std::function<void(const T&)>& visit) {
        for (const auto& child : node->children) postWalk(child.get(), visit);
        visit(node->value);
    }

    static std::size_t heightWalk(const Node* node) {
        std::size_t best = 0;
        for (const auto& child : node->children) {
            const std::size_t h = heightWalk(child.get()) + 1;
            if (h > best) best = h;
        }
        return best;
    }

    static void countLeaves(const Node* node, std::size_t& leaves) {
        if (node->children.empty()) {
            ++leaves;
            return;
        }
        for (const auto& child : node->children) countLeaves(child.get(), leaves);
    }

    std::unique_ptr<Node> root_;
    std::size_t count_ = 0;
};

} // namespace verity::ds
