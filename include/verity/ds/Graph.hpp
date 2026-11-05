#pragma once
// VERITY GRAPH — Graph<N,E>: adjacency-list graph over the handwritten
// HashTable (real engine path: the evidence graph itself is this template).
//
// Frozen policies (docs/CONTRACT.md §5):
//   * directed AND undirected supported (constructor flag)
//   * self-loops allowed; parallel edges allowed (removeEdge removes the
//     first match unless a specific weight is given — that overload requires
//     E to define operator==)
//   * degree conventions: undirected self-loop counts 2 in degree();
//     directed degree() == inDegree + outDegree (a loop contributes 1 to each)
//   * undirected edges are stored in both directions internally; edgeCount()
//     reports each undirected pair once (a loop counts once)
//
// Complexity summary (V = nodeCount, E = edgeCount):
//   addNode            O(1) amortized
//   addEdge            O(1) amortized
//   hasNode            O(1) average
//   neighbors/inNeighbors O(1) (reference) after O(1) lookup
//   degree/degrees     O(1)
//   removeEdge         O(deg(a))
//   removeNode         O(V + E)   (re-index + edge filtering)
//   transpose          O(V + E)
//   edgeCount          O(V)

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "verity/ds/HashTable.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

template <class N, class E>
class Graph {
public:
    /// Value-type edge descriptor for the addEdge overload (Phase 2 usage).
    struct Edge {
        N from;
        N to;
        E weight;
    };

    explicit Graph(bool directed = true) : directed_(directed) {}

    // --- nodes ---------------------------------------------------------------
    void addNode(const N& id) {
        if (index_.contains(id)) {
            throw util::DuplicateNodeError(nodeName(id));
        }
        index_.insert(id, nodes_.size());
        nodes_.push_back(id);
        adj_.emplace_back();
        radj_.emplace_back();
    }

    [[nodiscard]] bool hasNode(const N& id) const noexcept { return index_.contains(id); }

    [[nodiscard]] const std::vector<N>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] std::size_t nodeCount() const noexcept { return nodes_.size(); }

    // --- edges ---------------------------------------------------------------
    void addEdge(const N& a, const N& b, const E& w) {
        const std::size_t ia = indexOf(a);
        const std::size_t ib = indexOf(b);
        adj_[ia].emplace_back(b, w);
        radj_[ib].emplace_back(a, w);
    }

    /// Undirected convenience: records the edge in both directions.
    void addEdgeUndirected(const N& a, const N& b, const E& w) {
        const std::size_t ia = indexOf(a);
        const std::size_t ib = indexOf(b);
        adj_[ia].emplace_back(b, w);
        radj_[ia].emplace_back(b, w);
        if (a == b) {
            adj_[ia].emplace_back(a, w);
            radj_[ia].emplace_back(a, w);
        } else {
            adj_[ib].emplace_back(a, w);
            radj_[ib].emplace_back(a, w);
        }
    }

    /// Overloaded edge addition from an Edge value (directed semantics).
    void addEdge(const Edge& e) { addEdge(e.from, e.to, e.weight); }

    [[nodiscard]] bool hasEdge(const N& a, const N& b) const {
        const auto& out = neighbors(a);
        for (const auto& [to, weight] : out) {
            (void)weight;
            if (to == b) return true;
        }
        return false;
    }

    [[nodiscard]] const std::vector<std::pair<N, E>>& neighbors(const N& id) const {
        return adj_[indexOf(id)];
    }

    [[nodiscard]] const std::vector<std::pair<N, E>>& inNeighbors(const N& id) const {
        return radj_[indexOf(id)];
    }

    [[nodiscard]] std::size_t outDegree(const N& id) const { return neighbors(id).size(); }
    [[nodiscard]] std::size_t inDegree(const N& id) const { return inNeighbors(id).size(); }

    /// Undirected: |adj| (loop counts 2). Directed: in + out (loop 1+1).
    [[nodiscard]] std::size_t degree(const N& id) const {
        return directed_ ? outDegree(id) + inDegree(id) : outDegree(id);
    }

    /// Removes the first a->b edge; when `w` is given, removes the first edge
    /// whose weight equals *w (requires E to support operator==).
    bool removeEdge(const N& a, const N& b, const E* w = nullptr) {
        const std::size_t ia = indexOf(a);
        const std::size_t ib = indexOf(b);
        auto& out = adj_[ia];
        std::size_t found = out.size();
        for (std::size_t i = 0; i < out.size(); ++i) {
            if (out[i].first == b && (w == nullptr || out[i].second == *w)) {
                found = i;
                break;
            }
        }
        if (found == out.size()) return false;
        const E keptWeight = out[found].second;
        eraseAt(out, found);
        // remove the mirrored in-edge
        auto& in = radj_[ib];
        for (std::size_t i = 0; i < in.size(); ++i) {
            if (in[i].first == a && (w == nullptr || in[i].second == keptWeight)) {
                eraseAt(in, i);
                break;
            }
        }
        if (!directed_ && a != b) {
            // also remove b->a copy (adj_[ib]) and its mirror (radj_[ia])
            auto& outB = adj_[ib];
            for (std::size_t i = 0; i < outB.size(); ++i) {
                if (outB[i].first == a && outB[i].second == keptWeight) {
                    eraseAt(outB, i);
                    break;
                }
            }
            auto& inA = radj_[ia];
            for (std::size_t i = 0; i < inA.size(); ++i) {
                if (inA[i].first == b && inA[i].second == keptWeight) {
                    eraseAt(inA, i);
                    break;
                }
            }
        }
        return true;
    }

    /// Removes a node and every edge touching it. O(V + E) (re-indexes).
    bool removeNode(const N& id) {
        if (!index_.contains(id)) return false;
        const std::size_t victim = indexOf(id);
        nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(victim));
        adj_.erase(adj_.begin() + static_cast<std::ptrdiff_t>(victim));
        radj_.erase(radj_.begin() + static_cast<std::ptrdiff_t>(victim));
        for (auto& out : adj_) {
            removeFromList(out, id);
        }
        for (auto& in : radj_) {
            removeFromList(in, id);
        }
        index_.remove(id);
        index_.clear();
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            index_.insert(nodes_[i], i);
        }
        return true;
    }

    /// Directed reverse of every edge (an undirected graph transposes to an
    /// equivalent directed twin).
    [[nodiscard]] Graph transpose() const {
        Graph g(true);
        for (const N& n : nodes_) g.addNode(n);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            for (const auto& [to, w] : adj_[i]) {
                g.addEdge(to, nodes_[i], w);
            }
        }
        return g;
    }

    /// Weight of the FIRST a->b edge; throws MissingNodeError when absent.
    [[nodiscard]] E& operator[](const std::pair<N, N>& endpoints) {
        auto& out = adj_[indexOf(endpoints.first)];
        for (auto& [to, weight] : out) {
            if (to == endpoints.second) return weight;
        }
        throw util::MissingNodeError(
            "edge not found between '" + nodeName(endpoints.first) + "' and '" +
            nodeName(endpoints.second) + "'");
    }

    [[nodiscard]] std::size_t edgeCount() const {
        std::size_t total = 0;
        for (const auto& out : adj_) total += out.size();
        return directed_ ? total : total / 2;
    }

    [[nodiscard]] bool directed() const noexcept { return directed_; }

private:
    [[nodiscard]] std::size_t indexOf(const N& id) const {
        const std::size_t* pos = index_.get(id);
        if (pos == nullptr) {
            throw util::MissingNodeError(nodeName(id));
        }
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

    static void eraseAt(std::vector<std::pair<N, E>>& list, std::size_t at) {
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(at));
    }

    static void removeFromList(std::vector<std::pair<N, E>>& list, const N& id) {
        // erase-remove idiom (vector::resize would require the pair to be
        // default-constructible, which Node-keyed graphs must not demand)
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&id](const std::pair<N, E>& entry) {
                                      return entry.first == id;
                                  }),
                   list.end());
    }

    bool directed_;
    std::vector<N> nodes_;
    HashTable<N, std::size_t> index_;
    std::vector<std::vector<std::pair<N, E>>> adj_;
    std::vector<std::vector<std::pair<N, E>>> radj_;
};

} // namespace verity::ds
