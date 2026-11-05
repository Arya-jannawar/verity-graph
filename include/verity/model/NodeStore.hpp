#pragma once
// VERITY GRAPH — NodeStore: the owning container of all Node objects.
//
// THE Rule-of-Five teaching class (Phase 1): raw new/delete ownership with
// copy ctor / copy assignment / destructor / move ctor / move assignment all
// hand-written. Must stay leak-free under ASan and (where available) valgrind.
//
// Ownership rules (frozen):
//   * NodeStore owns its Node* exclusively (deep-copy on copy via clone()).
//   * The rest of the application passes std::unique_ptr<Node> into add().
//   * Duplicate ids throw; lookup goes through the handwritten ds::HashTable
//     (handwritten structures in real engine paths).
//
// Complexity: add O(1) amortized; find O(1) average (HashTable); copy O(n).

#include <memory>
#include <string>
#include <vector>

#include "verity/ds/HashTable.hpp"

namespace verity::model {

class Node;
class NodeId;

class NodeStore {
public:
    explicit NodeStore(std::string name = "default");

    // --- Rule of Five -------------------------------------------------------
    NodeStore(const NodeStore& other);              // deep copy via Node::clone()
    NodeStore& operator=(const NodeStore& other);   // copy-and-swap (self-assignment safe)
    NodeStore(NodeStore&& other) noexcept;
    NodeStore& operator=(NodeStore&& other) noexcept;
    ~NodeStore() noexcept;

    // --- ownership ----------------------------------------------------------
    /// Takes ownership; throws on null input or duplicate id. Returns the raw
    /// observation pointer (NOT owning; do not delete).
    Node* add(std::unique_ptr<Node> node);

    Node* find(const NodeId& id) noexcept;
    [[nodiscard]] const Node* find(const NodeId& id) const noexcept;

    template <class T>
    T* findAs(const NodeId& id) noexcept {
        return dynamic_cast<T*>(find(id));
    }

    template <class T>
    [[nodiscard]] const T* findAs(const NodeId& id) const noexcept {
        return dynamic_cast<const T*>(find(id));
    }

    [[nodiscard]] const std::vector<Node*>& all() const noexcept { return nodes_; }
    [[nodiscard]] std::size_t size() const noexcept { return nodes_.size(); }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }

private:
    void destroy() noexcept;

    std::string name_;
    std::vector<Node*> nodes_;                       // raw owned pointers
    ds::HashTable<std::string, std::size_t> index_;  // id -> position in nodes_
};

} // namespace verity::model
