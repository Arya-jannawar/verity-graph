#pragma once
// VERITY GRAPH — ds/GraphMatrix: adjacency-MATRIX variant behind the SAME
// interface as ds::Graph (Phase 4). For dense graphs the matrix wins on
// edge-existence queries and A^k walk counting; for sparse graphs the list
// wins on memory — the benchmarks quantify exactly that (docs/BENCHMARKS.md).
//
// Interface parity (frozen): the subset used by engine + algorithms —
// addNode, addEdge/addEdgeUndirected, hasNode, hasEdge, nodes, neighbors,
// inNeighbors, outDegree, inDegree, degree, removeEdge, removeNode,
// transpose, nodeCount, edgeCount. Policies match: parallel edges collapse
// to the FIRST weight inserted; self-loops allowed; degree conventions
// identical (undirected loop = 2).
//
// Complexity summary (V vertices; E stored matrix cells):
//   addNode           O(V)         (row growth)   [list: O(1) amortized]
//   addEdge           O(1)                        [list: O(1)]
//   hasEdge           O(1)                        [list: O(deg)]
//   neighbors         O(V) materialized copy      [list: O(1) reference]
//   removeEdge        O(1)                        [list: O(deg)]
//   removeNode        O(V^2)                      [list: O(V + E)]
//   memory            O(V^2)                      [list: O(V + E)]

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "verity/ds/HashTable.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

template <class N, class E>
class GraphMatrix {
public:
    struct Edge {
        N from;
        N to;
        E weight;
    };

    explicit GraphMatrix(bool directed = true) : directed_(directed) {}

    void addNode(const N& id) {
        if (index_.contains(id)) {
            throw util::DuplicateNodeError(nodeName(id));
        }
        index_.insert(id, nodes_.size());
        nodes_.push_back(id);
        for (auto& row : matrix_) row.emplace_back(false, E{});
        matrix_.emplace_back(nodes_.size(), std::make_pair(false, E{}));
    }

    [[nodiscard]] bool hasNode(const N& id) const noexcept { return index_.contains(id); }
    [[nodiscard]] const std::vector<N>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] std::size_t nodeCount() const noexcept { return nodes_.size(); }

    void addEdge(const N& a, const N& b, const E& w) {
        auto& cell = matrix_[indexOf(a)][indexOf(b)];
        if (!cell.first) cell = {true, w};   // first parallel edge wins
        if (!directed_) {
            auto& mirror = matrix_[indexOf(b)][indexOf(a)];
            if (!mirror.first) mirror = {true, w};
        }
    }

    void addEdgeUndirected(const N& a, const N& b, const E& w) {
        auto& cell = matrix_[indexOf(a)][indexOf(b)];
        if (!cell.first) cell = {true, w};
        auto& mirror = matrix_[indexOf(b)][indexOf(a)];
        if (!mirror.first) mirror = {true, w};
    }

    [[nodiscard]] bool hasEdge(const N& a, const N& b) const {
        return matrix_[indexOf(a)][indexOf(b)].first;
    }

    /// Materialized neighbor list (O(V)); only present edges are included.
    [[nodiscard]] std::vector<std::pair<N, E>> neighbors(const N& id) const {
        const std::size_t i = indexOf(id);
        std::vector<std::pair<N, E>> out;
        for (std::size_t j = 0; j < nodes_.size(); ++j) {
            if (matrix_[i][j].first) out.emplace_back(nodes_[j], matrix_[i][j].second);
        }
        return out;
    }

    [[nodiscard]] std::vector<std::pair<N, E>> inNeighbors(const N& id) const {
        const std::size_t j = indexOf(id);
        std::vector<std::pair<N, E>> out;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            if (matrix_[i][j].first) out.emplace_back(nodes_[i], matrix_[i][j].second);
        }
        return out;
    }

    [[nodiscard]] std::size_t outDegree(const N& id) const {
        const std::size_t i = indexOf(id);
        std::size_t count = 0;
        for (std::size_t j = 0; j < nodes_.size(); ++j) count += matrix_[i][j].first ? 1u : 0u;
        return count;
    }

    [[nodiscard]] std::size_t inDegree(const N& id) const {
        const std::size_t j = indexOf(id);
        std::size_t count = 0;
        for (std::size_t i = 0; i < nodes_.size(); ++i) count += matrix_[i][j].first ? 1u : 0u;
        return count;
    }

    /// Same convention as ds::Graph: undirected degree counts a loop twice.
    [[nodiscard]] std::size_t degree(const N& id) const {
        const std::size_t i = indexOf(id);
        std::size_t count = 0;
        for (std::size_t j = 0; j < nodes_.size(); ++j) {
            if (matrix_[i][j].first) count += (i == j ? 2u : 1u);
        }
        return count;
    }

    bool removeEdge(const N& a, const N& b) {
        auto& cell = matrix_[indexOf(a)][indexOf(b)];
        const bool had = cell.first;
        cell = {false, E{}};
        if (!directed_) {
            auto& mirror = matrix_[indexOf(b)][indexOf(a)];
            mirror = {false, E{}};
        }
        return had;
    }

    bool removeNode(const N& id) {
        if (!index_.contains(id)) return false;
        const std::size_t victim = indexOf(id);
        nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(victim));
        matrix_.erase(matrix_.begin() + static_cast<std::ptrdiff_t>(victim));
        for (auto& row : matrix_) row.erase(row.begin() + static_cast<std::ptrdiff_t>(victim));
        index_.remove(id);
        index_.clear();
        for (std::size_t i = 0; i < nodes_.size(); ++i) index_.insert(nodes_[i], i);
        return true;
    }

    [[nodiscard]] GraphMatrix transpose() const {
        GraphMatrix g(true);
        for (const N& n : nodes_) g.addNode(n);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            for (std::size_t j = 0; j < nodes_.size(); ++j) {
                if (matrix_[i][j].first) g.addEdge(nodes_[j], nodes_[i], matrix_[i][j].second);
            }
        }
        return g;
    }

    [[nodiscard]] std::size_t edgeCount() const {
        if (directed_) return countTotal();
        // undirected: each pair once, plus each self-loop once
        std::size_t count = 0;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            for (std::size_t j = i + 1; j < nodes_.size(); ++j) {
                if (matrix_[i][j].first) ++count;
            }
            if (matrix_[i][i].first) ++count;
        }
        return count;
    }

    [[nodiscard]] bool directed() const noexcept { return directed_; }

private:
    [[nodiscard]] std::size_t countTotal() const {
        std::size_t count = 0;
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            for (std::size_t j = 0; j < nodes_.size(); ++j) {
                if (matrix_[i][j].first) ++count;
            }
        }
        return count;
    }

    [[nodiscard]] std::size_t indexOf(const N& id) const {
        const std::size_t* pos = index_.get(id);
        if (pos == nullptr) throw util::MissingNodeError(nodeName(id));
        return *pos;
    }

    [[nodiscard]] static std::string nodeName(const N& id) {
        if constexpr (std::is_constructible_v<std::string, const N&>) {
            return std::string(id);
        } else {
            (void)id;
            return std::string("<node>");
        }
    }

    bool directed_;
    std::vector<N> nodes_;
    HashTable<N, std::size_t> index_;
    std::vector<std::vector<std::pair<bool, E>>> matrix_;
};

} // namespace verity::ds
