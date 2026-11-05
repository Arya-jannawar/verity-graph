#include "verity/model/NodeStore.hpp"

#include <algorithm>
#include <utility>

#include "verity/model/Node.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::model {

NodeStore::NodeStore(std::string name) : name_(std::move(name)) {}

// Deep copy: clone every owned node in insertion order.
NodeStore::NodeStore(const NodeStore& other)
    : name_(other.name_ + "_copy") {
    nodes_.reserve(other.nodes_.size());
    try {
        for (const Node* n : other.nodes_) {
            nodes_.push_back(n->clone().release());
        }
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            index_.insert(nodes_[i]->id().str(), i);
        }
    } catch (...) {
        destroy();
        throw;
    }
}

// Copy-and-swap: strong exception safety + implicit self-assignment safety.
NodeStore& NodeStore::operator=(const NodeStore& other) {
    if (this != &other) {
        NodeStore tmp(other);
        *this = std::move(tmp);
    }
    return *this;
}

// Move: steal internals, leave `other` empty-but-valid.
NodeStore::NodeStore(NodeStore&& other) noexcept
    : name_(std::move(other.name_)),
      nodes_(std::move(other.nodes_)),
      index_(std::move(other.index_)) {
    other.name_ = "moved-from";
}

NodeStore& NodeStore::operator=(NodeStore&& other) noexcept {
    if (this != &other) {
        destroy();
        name_ = std::move(other.name_);
        nodes_ = std::move(other.nodes_);
        index_ = std::move(other.index_);
        other.name_ = "moved-from";
    }
    return *this;
}

NodeStore::~NodeStore() noexcept { destroy(); }

void NodeStore::destroy() noexcept {
    for (Node* n : nodes_) {
        delete n;
    }
    nodes_.clear();
    index_.clear();
}

Node* NodeStore::add(std::unique_ptr<Node> node) {
    if (!node) {
        throw util::VerityError("NodeStore::add called with null node");
    }
    Node* raw = node.get();
    const std::string key = raw->id().str();
    if (index_.contains(key)) {
        throw util::DuplicateNodeError(key);
    }
    const std::size_t position = nodes_.size();
    nodes_.push_back(raw);              // take ownership on success only
    (void)node.release();
    index_.insert(key, position);       // cannot throw duplicate: checked above
    return raw;
}

Node* NodeStore::find(const NodeId& id) noexcept {
    const std::size_t* pos = index_.get(id.str());
    return pos ? nodes_[*pos] : nullptr;
}

const Node* NodeStore::find(const NodeId& id) const noexcept {
    const std::size_t* pos = index_.get(id.str());
    return pos ? nodes_[*pos] : nullptr;
}

} // namespace verity::model
