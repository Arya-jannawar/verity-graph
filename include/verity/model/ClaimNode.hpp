#pragma once
// VERITY GRAPH — ClaimNode: a sourced assertion "subject predicate object".
//
// OOP coverage:
//   * encapsulation with VALIDATED setter (setConfidence throws outside [0,1])
//   * constructor overloading + default arguments
//   * this-pointer method chaining (withConfidence/withPolarity/withValidTime)
//
// Complexity: all methods O(1).

#include <istream>
#include <optional>
#include <string>

#include "verity/model/Enums.hpp"
#include "verity/model/Extraction.hpp"   // Span, ClaimObject, DateInterval
#include "verity/model/Interfaces.hpp"   // IScorable
#include "verity/model/Node.hpp"

namespace verity::model {

class ClaimNode : public Node, public IScorable {
public:
    /// Full constructor (default arguments demonstrate the overload set).
    explicit ClaimNode(std::string label, NodeId subjectId, std::string predicate,
                       ClaimObject object, NodeId documentId, Span span,
                       Polarity polarity = Polarity::Positive,
                       double confidence = 0.8,
                       const std::optional<std::string>& id = std::nullopt,
                       std::optional<DateInterval> validTime = std::nullopt);

    /// Overload: string-valued object convenience constructor.
    ClaimNode(std::string label, NodeId subjectId, std::string predicate,
              std::string objectText, NodeId documentId, Span span,
              Polarity polarity = Polarity::Positive,
              double confidence = 0.8,
              const std::optional<std::string>& id = std::nullopt);

    [[nodiscard]] const NodeId& subjectId() const noexcept { return subjectId_; }
    [[nodiscard]] const std::string& predicate() const noexcept { return predicate_; }
    [[nodiscard]] const ClaimObject& object() const noexcept { return object_; }
    [[nodiscard]] Polarity polarity() const noexcept { return polarity_; }
    [[nodiscard]] double confidence() const noexcept { return confidence_; }
    [[nodiscard]] const NodeId& documentId() const noexcept { return documentId_; }
    [[nodiscard]] const Span& span() const noexcept { return span_; }
    [[nodiscard]] const std::optional<DateInterval>& validTime() const noexcept { return validTime_; }

    /// VALIDATED setter: throws VerityError when confidence is outside [0,1].
    void setConfidence(double confidence);
    void setPolarity(Polarity p) noexcept { polarity_ = p; }
    void setValidTime(DateInterval iv) { validTime_ = std::move(iv); }

    // this-pointer chaining
    ClaimNode& withConfidence(double c);
    ClaimNode& withPolarity(Polarity p) noexcept;
    ClaimNode& withValidTime(DateInterval iv);

    [[nodiscard]] std::string type() const override { return "Claim"; }
    [[nodiscard]] std::string describe() const override;
    [[nodiscard]] nlohmann::json toJson() const override;
    [[nodiscard]] std::unique_ptr<Node> clone() const override {
        return std::make_unique<ClaimNode>(*this);
    }
    /// IScorable: a claim's score is its extraction confidence.
    [[nodiscard]] double score() const override { return confidence_; }

    /// Short human rendering of a ClaimObject ("E002" | "text" | date | qty).
    [[nodiscard]] static std::string objectToString(const ClaimObject& o);

    friend class GraphInspector;   // private-state introspection (tests/debug)

    // --- Phase-2 operator overloads ------------------------------------------
    /// Stream extraction: reads a confidence value and applies the validated
    /// setter (demonstration of >> with encapsulated validation).
    friend std::istream& operator>>(std::istream& is, ClaimNode& claim) {
        double value = 0.0;
        if (is >> value) claim.setConfidence(value);
        return is;
    }

private:
    NodeId subjectId_;
    std::string predicate_;
    ClaimObject object_;
    Polarity polarity_;
    double confidence_;
    NodeId documentId_;
    Span span_;
    std::optional<DateInterval> validTime_;
};

} // namespace verity::model
