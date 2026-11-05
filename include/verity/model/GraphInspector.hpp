#pragma once
// VERITY GRAPH — GraphInspector: friend-class introspection demo.
//
// Deliberately a *friend* of the Node family (NOT using public API) so the
// codebase demonstrates friend classes accessing private state — used by
// tests and debug tooling, never by engine logic.
//
// Complexity: O(1) per inspection.

#include <string>

#include <nlohmann/json.hpp>

#include "verity/model/ClaimNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/Node.hpp"

namespace verity::model {

class GraphInspector {
public:
    /// Full private-state dump of any Node (dispatches on dynamic type).
    [[nodiscard]] static nlohmann::json inspect(const Node& node) {
        nlohmann::json j;
        j["id"] = node.id_.str();
        j["label"] = node.label_;
        j["dynamic_type"] = node.type();
        if (const auto* e = dynamic_cast<const Entity*>(&node)) {
            j["entity_type"] = to_string(e->entityType_);
            j["aliases"] = e->aliases_;
        } else if (const auto* c = dynamic_cast<const ClaimNode*>(&node)) {
            j["subject_id"] = c->subjectId_.str();
            j["predicate"] = c->predicate_;
            j["polarity"] = to_string(c->polarity_);
            j["confidence"] = c->confidence_;
            j["document_id"] = c->documentId_.str();
        }
        return j;
    }

    /// Direct private-member access demonstration (friend privilege).
    [[nodiscard]] static double confidenceOf(const ClaimNode& claim) {
        return claim.confidence_;
    }

    /// Entity alias list from private state.
    [[nodiscard]] static std::string firstAliasOf(const Entity& entity) {
        return entity.aliases_.empty() ? std::string() : entity.aliases_.front();
    }
};

} // namespace verity::model
