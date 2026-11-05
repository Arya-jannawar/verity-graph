#pragma once
// VERITY GRAPH — Entity node: a canonical actor in the evidence graph.
//
// Phase-2 operators: == (identity by name+type+aliases), < (name then type
// then id), + (alias union). The MERGE DOMAIN for the alias-union monoid is
// the alias multiset (AliasSet), NOT raw Entity objects: AliasSet has an
// empty identity {} and associative union, so (AliasSet, +, {}) is a genuine
// commutative monoid (tested in test_operators). Entity::+ requires matching
// names and appends the union of aliases.
//
// Complexity: operators O(aliases); describe/toJson O(aliases).

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "verity/model/Enums.hpp"
#include "verity/model/Node.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::model {

/// The alias aggregate: the proper domain of the alias-union monoid.
struct AliasSet {
    std::set<std::string> aliases;

    AliasSet& operator+=(const AliasSet& other) {
        aliases.insert(other.aliases.begin(), other.aliases.end());
        return *this;
    }
    [[nodiscard]] friend AliasSet operator+(AliasSet a, const AliasSet& b) {
        a += b;
        return a;
    }
    [[nodiscard]] bool operator==(const AliasSet& other) const noexcept {
        return aliases == other.aliases;
    }
    [[nodiscard]] static const AliasSet& identity() noexcept {
        static const AliasSet empty;
        return empty;
    }
};

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

    // --- Phase-2 operators ---------------------------------------------------
    /// Identity: same name, same type, same alias set (id-independent so a
    /// mention entity and its canonical twin compare equal pre-merge).
    [[nodiscard]] friend bool operator==(const Entity& a, const Entity& b) {
        if (a.label() != b.label() || a.entityType_ != b.entityType_) return false;
        AliasSet sa;
        sa.aliases.insert(a.aliases_.begin(), a.aliases_.end());
        AliasSet sb;
        sb.aliases.insert(b.aliases_.begin(), b.aliases_.end());
        return sa == sb;
    }
    [[nodiscard]] friend bool operator!=(const Entity& a, const Entity& b) { return !(a == b); }

    /// Ordering: name, then type, then id (deterministic entity lists).
    [[nodiscard]] friend bool operator<(const Entity& a, const Entity& b) {
        if (a.label() != b.label()) return a.label() < b.label();
        if (a.entityType_ != b.entityType_) return a.entityType_ < b.entityType_;
        return a.id().str() < b.id().str();
    }

    /// Alias union: requires matching non-empty names; result keeps this
    /// entity's name/type and the union of both alias sets.
    [[nodiscard]] friend Entity operator+(const Entity& a, const Entity& b) {
        if (a.label() != b.label()) {
            throw util::VerityError("Entity + requires matching names ('" + a.label() +
                                    "' vs '" + b.label() + "')");
        }
        Entity merged = a;
        for (const std::string& alias : b.aliases_) merged.addAlias(alias);
        return merged;
    }

private:
    EntityType entityType_;
    std::vector<std::string> aliases_;
};

} // namespace verity::model
