#pragma once
// VERITY GRAPH — model/Evidence: polymorphic evidence family with SIBLING
// classes (OOP Unit 3, Phase 2) — SupportingEvidence, ContradictingEvidence,
// NeutralEvidence are siblings under abstract Evidence (NO chain of
// inheritance among them; the family splits at the abstract root).
//
// Operators: == compares every field; < orders by (claim, document, kind,
// score) for deterministic evidence lists; the IScorable interface supplies
// score(). Phase 3's EvidenceBuilder instantiates these.
//
// Complexity: all methods O(1).

#include <compare>
#include <string>

#include "verity/model/Enums.hpp"
#include "verity/model/Interfaces.hpp"
#include "verity/model/NodeId.hpp"

namespace verity::model {

/// Abstract evidence root: links a claim to a document with a stance.
class Evidence : public IScorable, public IExportable {
public:
    Evidence(NodeId claimId, NodeId documentId, double score)
        : claimId_(std::move(claimId)), documentId_(std::move(documentId)), score_(score) {}
    virtual ~Evidence() noexcept = default;

    [[nodiscard]] const NodeId& claimId() const noexcept { return claimId_; }
    [[nodiscard]] const NodeId& documentId() const noexcept { return documentId_; }
    [[nodiscard]] virtual EvidenceKind kind() const noexcept = 0;
    [[nodiscard]] virtual std::string kindName() const noexcept = 0;
    [[nodiscard]] double score() const override { return score_; }

    [[nodiscard]] bool operator==(const Evidence& other) const {
        return claimId_ == other.claimId_ && documentId_ == other.documentId_ &&
               kind() == other.kind() && score_ == other.score_;
    }

    [[nodiscard]] bool operator<(const Evidence& other) const {
        const std::string a = claimId_.str() + "|" + documentId_.str() + "|" + kindName();
        const std::string b = other.claimId_.str() + "|" + other.documentId_.str() + "|" +
                              other.kindName();
        if (a != b) return a < b;
        return score_ < other.score_;
    }

protected:
    NodeId claimId_;
    NodeId documentId_;
    double score_;
};

class SupportingEvidence final : public Evidence {
public:
    using Evidence::Evidence;
    [[nodiscard]] EvidenceKind kind() const noexcept override { return EvidenceKind::Supporting; }
    [[nodiscard]] std::string kindName() const noexcept override { return "Supporting"; }
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] std::string toDot() const override;
};

class ContradictingEvidence final : public Evidence {
public:
    using Evidence::Evidence;
    [[nodiscard]] EvidenceKind kind() const noexcept override { return EvidenceKind::Contradicting; }
    [[nodiscard]] std::string kindName() const noexcept override { return "Contradicting"; }
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] std::string toDot() const override;
};

class NeutralEvidence final : public Evidence {
public:
    using Evidence::Evidence;
    [[nodiscard]] EvidenceKind kind() const noexcept override { return EvidenceKind::Neutral; }
    [[nodiscard]] std::string kindName() const noexcept override { return "Neutral"; }
    [[nodiscard]] std::string toJson() const override;
    [[nodiscard]] std::string toDot() const override;
};

} // namespace verity::model
