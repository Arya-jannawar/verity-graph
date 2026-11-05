#pragma once
// VERITY GRAPH — model/Interfaces: IScorable and IExportable (OOP Unit 3).
//
// Pure interface classes (no data, virtual destructors). The Evidence family
// implements IExportable (toJson/toDot) and IScorable (score); ClaimNode
// implements IScorable (score = confidence), which lets the Phase-3 ranker
// order any scorable object through one interface.
//
// Note: IExportable::toJson intentionally returns std::string and is NOT
// mixed onto Node-derived classes (Node::toJson returns nlohmann::json; two
// same-name virtuals with different return types cannot coexist).

#include <string>

namespace verity::model {

class IScorable {
public:
    virtual ~IScorable() noexcept = default;
    [[nodiscard]] virtual double score() const = 0;
};

class IExportable {
public:
    virtual ~IExportable() noexcept = default;
    [[nodiscard]] virtual std::string toJson() const = 0;
    [[nodiscard]] virtual std::string toDot() const = 0;
};

} // namespace verity::model
