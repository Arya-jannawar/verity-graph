#pragma once
// VERITY GRAPH — frozen enums (docs/CONTRACT.md §3).
// Serialized values are exactly the enumerator names, e.g. "SubjectOf".

#include <string>

#include "verity/util/VerityError.hpp"

namespace verity::model {

enum class EntityType { Person, Organization, Product, Location };

enum class RelationType {
    SubjectOf,        // claim -> subject entity
    ObjectOf,         // claim -> object entity (entity references)
    MentionedIn,      // entity -> document (mention evidence)
    DocumentEvidence, // evidence/claim -> document support link
    Cites,            // document -> document citation
    AliasOf,          // mention entity -> canonical entity
    HappenedBefore,   // event -> event strict order claim
    DerivedFrom       // derivation provenance link
};

enum class Polarity { Positive, Negative, Unknown };
enum class EvidenceKind { Supporting, Contradicting, Neutral };
enum class DocumentFormat { Txt, Pdf, Docx };

[[nodiscard]] inline const char* to_string(EntityType t) noexcept {
    switch (t) {
        case EntityType::Person:        return "Person";
        case EntityType::Organization:  return "Organization";
        case EntityType::Product:       return "Product";
        case EntityType::Location:      return "Location";
    }
    return "Person";
}

[[nodiscard]] inline const char* to_string(RelationType r) noexcept {
    switch (r) {
        case RelationType::SubjectOf:        return "SubjectOf";
        case RelationType::ObjectOf:         return "ObjectOf";
        case RelationType::MentionedIn:      return "MentionedIn";
        case RelationType::DocumentEvidence: return "DocumentEvidence";
        case RelationType::Cites:            return "Cites";
        case RelationType::AliasOf:          return "AliasOf";
        case RelationType::HappenedBefore:   return "HappenedBefore";
        case RelationType::DerivedFrom:      return "DerivedFrom";
    }
    return "SubjectOf";
}

[[nodiscard]] inline const char* to_string(Polarity p) noexcept {
    switch (p) {
        case Polarity::Positive: return "Positive";
        case Polarity::Negative: return "Negative";
        case Polarity::Unknown:  return "Unknown";
    }
    return "Unknown";
}

[[nodiscard]] inline const char* to_string(EvidenceKind k) noexcept {
    switch (k) {
        case EvidenceKind::Supporting:    return "Supporting";
        case EvidenceKind::Contradicting: return "Contradicting";
        case EvidenceKind::Neutral:       return "Neutral";
    }
    return "Neutral";
}

[[nodiscard]] inline EntityType entityTypeFromString(const std::string& s) {
    if (s == "Person") return EntityType::Person;
    if (s == "Organization") return EntityType::Organization;
    if (s == "Product") return EntityType::Product;
    if (s == "Location") return EntityType::Location;
    throw util::ParseError("Unknown EntityType: '" + s + "'");
}

[[nodiscard]] inline RelationType relationTypeFromString(const std::string& s) {
    if (s == "SubjectOf") return RelationType::SubjectOf;
    if (s == "ObjectOf") return RelationType::ObjectOf;
    if (s == "MentionedIn") return RelationType::MentionedIn;
    if (s == "DocumentEvidence") return RelationType::DocumentEvidence;
    if (s == "Cites") return RelationType::Cites;
    if (s == "AliasOf") return RelationType::AliasOf;
    if (s == "HappenedBefore") return RelationType::HappenedBefore;
    if (s == "DerivedFrom") return RelationType::DerivedFrom;
    throw util::ParseError("Unknown RelationType: '" + s + "'");
}

[[nodiscard]] inline Polarity polarityFromString(const std::string& s) {
    if (s == "Positive") return Polarity::Positive;
    if (s == "Negative") return Polarity::Negative;
    if (s == "Unknown") return Polarity::Unknown;
    throw util::ParseError("Unknown Polarity: '" + s + "'");
}

[[nodiscard]] inline EvidenceKind evidenceKindFromString(const std::string& s) {
    if (s == "Supporting") return EvidenceKind::Supporting;
    if (s == "Contradicting") return EvidenceKind::Contradicting;
    if (s == "Neutral") return EvidenceKind::Neutral;
    throw util::ParseError("Unknown EvidenceKind: '" + s + "'");
}

[[nodiscard]] inline const char* to_string(DocumentFormat f) noexcept {
    switch (f) {
        case DocumentFormat::Txt:  return "txt";
        case DocumentFormat::Pdf:  return "pdf";
        case DocumentFormat::Docx: return "docx";
    }
    return "txt";
}

[[nodiscard]] inline DocumentFormat documentFormatFromString(const std::string& s) {
    if (s == "txt") return DocumentFormat::Txt;
    if (s == "pdf") return DocumentFormat::Pdf;
    if (s == "docx") return DocumentFormat::Docx;
    throw util::ParseError("Unknown DocumentFormat: '" + s + "'");
}

} // namespace verity::model
