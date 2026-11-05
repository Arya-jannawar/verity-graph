#pragma once
// VERITY GRAPH — ds/Trees: tree-theory checks and the arithmetic expression
// tree (DM Unit 6, Phase 4).
//
//   * isTree: connected + |E| == |V| - 1 (checked on an undirected projection)
//   * fullBinaryLeaves: a full binary tree with i internal nodes has i + 1 leaves
//   * ArithmeticExpressionTree: expression tree with pre/in/post-order walks
//     and evaluation (used by the DM Lab and the HTML exporter's sample)
//   * spanning tree verification: a candidate edge set is a spanning tree iff
//     it connects all n vertices with exactly n - 1 edges (brute-force oracle
//     for Kruskal/Prim cross-checks)
//
// Complexity: isTree O(V + E); expression evaluation O(nodes).

#include <functional>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/Heap.hpp"
#include "verity/ds/Queue.hpp"
#include "verity/ds/Stack.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

/// |E| == |V| - 1 AND connected (BFS from the first node reaches everything).
template <class N, class E>
[[nodiscard]] bool isTree(const Graph<N, E>& graph) {
    const std::size_t n = graph.nodeCount();
    if (graph.edgeCount() != n - 1) return false;
    if (n == 0) return false;
    // BFS over the undirected sense of the edges
    HashTable<N, bool> seen;
    Queue<N> frontier;
    seen.insert(graph.nodes().front(), true);
    frontier.enqueue(graph.nodes().front());
    std::size_t reached = 0;
    while (!frontier.empty()) {
        const N node = frontier.dequeue();
        ++reached;
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            if (!seen.contains(to)) {
                seen.insert(to, true);
                frontier.enqueue(to);
            }
        }
        // walk in-edges too so direction does not hide connectivity
        for (const auto& [from, w] : graph.inNeighbors(node)) {
            (void)w;
            if (!seen.contains(from)) {
                seen.insert(from, true);
                frontier.enqueue(from);
            }
        }
    }
    return reached == n;
}

/// Full binary tree with i internal nodes has exactly i + 1 leaves.
/// Verified structurally over a provided leaf/internal count pair.
[[nodiscard]] inline bool fullBinaryLeafLaw(std::size_t internalNodes,
                                            std::size_t leaves) noexcept {
    return leaves == internalNodes + 1;
}

/// Binary arithmetic expression tree.
class ExpressionTree {
public:
    struct Node {
        std::string token;                    // number or operator (+ - * /)
        std::unique_ptr<Node> left, right;
        bool isOperator() const noexcept { return token == "+" || token == "-" ||
                                                 token == "*" || token == "/"; }
    };

    /// Builds from a postfix token list (RPN). Throws ParseError on malformed
    /// input (wrong arity, unknown token, leftover operands).
    [[nodiscard]] static ExpressionTree fromPostfix(const std::vector<std::string>& tokens) {
        ExpressionTree tree;
        Stack<Node*> build;
        for (const std::string& token : tokens) {
            const bool isOp = token == "+" || token == "-" || token == "*" || token == "/";
            if (isOp) {
                if (build.size() < 2) {
                    throw util::ParseError("expression tree: operator '" + token +
                                           "' lacks operands");
                }
                auto node = std::make_unique<Node>();
                node->token = token;
                node->right.reset(build.pop());
                node->left.reset(build.pop());
                build.push(node.get());
                tree.roots_.push_back(std::move(node));
            } else {
                auto node = std::make_unique<Node>();
                node->token = token;
                build.push(node.get());
                tree.roots_.push_back(std::move(node));
            }
            ++tree.count_;
        }
        if (build.size() != 1) {
            throw util::ParseError("expression tree: leftover operands");
        }
        tree.root_.reset(build.pop());
        tree.roots_.clear();
        return tree;
    }

    [[nodiscard]] const Node* root() const noexcept { return root_.get(); }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    /// In-order with parentheses (fully parenthesized infix form).
    [[nodiscard]] std::string infix() const { return infixWalk(root_.get()); }
    [[nodiscard]] std::string prefix() const { return prefixWalk(root_.get()); }
    [[nodiscard]] std::string postfix() const { return postfixWalk(root_.get()); }

    /// Integer evaluation (division truncates; ParseError on division by zero
    /// or non-numeric leaves).
    [[nodiscard]] long long evaluate() const { return evalWalk(root_.get()); }

    /// Leaf/internal counts for the full-binary law (a well-formed arithmetic
    /// tree IS a full binary tree: every operator node has exactly 2 children).
    [[nodiscard]] std::pair<std::size_t, std::size_t> internalAndLeaves() const {
        std::size_t internal = 0, leaves = 0;
        countWalk(root_.get(), internal, leaves);
        return {internal, leaves};
    }

private:
    ExpressionTree() = default;

    static std::string infixWalk(const Node* n) {
        if (n == nullptr) return "";
        if (!n->isOperator()) return n->token;
        return "(" + infixWalk(n->left.get()) + " " + n->token + " " +
               infixWalk(n->right.get()) + ")";
    }
    static std::string prefixWalk(const Node* n) {
        if (n == nullptr) return "";
        if (!n->isOperator()) return n->token;
        return n->token + " " + prefixWalk(n->left.get()) + " " + prefixWalk(n->right.get());
    }
    static std::string postfixWalk(const Node* n) {
        if (n == nullptr) return "";
        if (!n->isOperator()) return n->token;
        return postfixWalk(n->left.get()) + " " + postfixWalk(n->right.get()) + " " + n->token;
    }
    static long long evalWalk(const Node* n) {
        if (!n->isOperator()) {
            try {
                return std::stoll(n->token);
            } catch (const std::exception&) {
                throw util::ParseError("expression tree leaf is not a number: '" + n->token + "'");
            }
        }
        const long long l = evalWalk(n->left.get());
        const long long r = evalWalk(n->right.get());
        if (n->token == "+") return l + r;
        if (n->token == "-") return l - r;
        if (n->token == "*") return l * r;
        if (r == 0) throw util::ParseError("expression tree division by zero");
        return l / r;
    }
    static void countWalk(const Node* n, std::size_t& internal, std::size_t& leaves) {
        if (n == nullptr) return;
        if (n->isOperator()) {
            ++internal;
            countWalk(n->left.get(), internal, leaves);
            countWalk(n->right.get(), internal, leaves);
        } else {
            ++leaves;
        }
    }

    std::unique_ptr<Node> root_;
    std::vector<std::unique_ptr<Node>> roots_;   // keeps nodes alive during build
    std::size_t count_ = 0;
};

/// Verifies a candidate spanning tree: connects all n vertices with n-1 edges
/// (the brute-force oracle for Kruskal/Prim cross-checks).
template <class N, class E>
[[nodiscard]] bool isSpanningTree(const Graph<N, E>& graph,
                                  const std::vector<std::pair<N, N>>& candidateEdges) {
    const std::size_t n = graph.nodeCount();
    if (candidateEdges.size() != n - 1) return false;
    HashTable<N, bool> seen;
    Queue<N> frontier;
    if (n == 0) return false;
    seen.insert(graph.nodes().front(), true);
    frontier.enqueue(graph.nodes().front());
    std::size_t reached = 0;
    while (!frontier.empty()) {
        const N node = frontier.dequeue();
        ++reached;
        for (const auto& [a, b] : candidateEdges) {
            N next;
            if (a == node) next = b;
            else if (b == node) next = a;
            else continue;
            if (!seen.contains(next)) {
                seen.insert(next, true);
                frontier.enqueue(next);
            }
        }
    }
    return reached == n;
}

} // namespace verity::ds
