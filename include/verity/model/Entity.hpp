#pragma once
// VERITY GRAPH — Entity node: a canonical actor in the evidence graph.
//
// Phase-1 scope: construction, validated mutation, describe/toJson/clone.
// Phase 2 adds operators (==, <, alias-union +) on top of this class.
//
// Complexity: all methods O(1) except toJson/describe O(aliases).

#include <optional>
#include <string>
#include <vector>

#include "verity/model/Enums.hpp"
#include "verity/model/Node.hpp"

namespace verity::model {

class Entity : public Node {
public:
    /// Auto-generated id (E001...) when `id` is empty.
    Entity(std::string name, EntityType entityType,
           std::vector<std::string> aliases = {},
           const std::optional<std::string>& id = std::nullopt)
        : Node(NodeId(id.has_value() ? *id : nextId("E")),
               std::move(name)),
          entityType_(entityType),
          aliases_(std::move(aliases)) {}

    [[nodiscard]] EntityType entityType() const noexcept { return entityType_; }
    void setEntityType(EntityType t) noexcept { entityType_ = t; }

    [[nodiscard]] const std::vector<std::string>& aliases() const noexcept { return aliases_; }
    void addAlias(std::string alias) {
        if (!alias.empty()) aliases_.push_back(std::move(alias));
    }

    [[nodiscard]] std::string type() const override { return "Entity"; }

    [[nodiscard]] std::string describe() const override {
        std::string out = id().str() + " Entity[" + to_string(entityType_) +
                          "] '" + label() + "'";
        if (!aliases_.empty()) {
            out += " aliases:";
            for (std::size_t i = 0; i < aliases_.size(); ++i) {
                out += (i ? ", " : " ") + aliases_[i];
            }
        }
        return out;
    }

    [[nodiscard]] nlohmann::json toJson() const override {
        nlohmann::json j;
        j["id"] = id().str();
        j["label"] = label();
        j["type"] = "Entity";
        j["entity_type"] = to_string(entityType_);
        j["aliases"] = aliases_;
        return j;
    }

    [[nodiscard]] std::unique_ptr<Node> clone() const override {
        return std::make_unique<Entity>(*this);
    }

    friend class GraphInspector;   // private-state introspection (tests/debug)

private:
    EntityType entityType_;
    std::vector<std::string> aliases_;
};

} // namespace verity::model
