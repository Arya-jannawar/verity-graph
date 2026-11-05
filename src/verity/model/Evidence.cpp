#include "verity/model/Evidence.hpp"

namespace verity::model {

namespace {
std::string evidenceJson(const Evidence& e) {
    return std::string("{\"claim\": \"") + e.claimId().str() + "\", \"document\": \"" +
           e.documentId().str() + "\", \"kind\": \"" + e.kindName() +
           "\", \"score\": " + std::to_string(e.score()) + "}";
}

std::string evidenceDot(const Evidence& e) {
    return std::string("\"") + e.documentId().str() + "\" -> \"" + e.claimId().str() +
           "\" [label=\"" + e.kindName() + "\"];";
}
} // namespace

std::string SupportingEvidence::toJson() const { return evidenceJson(*this); }
std::string SupportingEvidence::toDot() const { return evidenceDot(*this); }

std::string ContradictingEvidence::toJson() const { return evidenceJson(*this); }
std::string ContradictingEvidence::toDot() const { return evidenceDot(*this); }

std::string NeutralEvidence::toJson() const { return evidenceJson(*this); }
std::string NeutralEvidence::toDot() const { return evidenceDot(*this); }

} // namespace verity::model
