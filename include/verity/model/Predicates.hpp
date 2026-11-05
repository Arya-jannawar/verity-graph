#pragma once
// VERITY GRAPH — frozen starter predicate vocabulary (docs/CONTRACT.md §9).
// Mirrors data/rules/predicates.json; io::JsonLoader rejects unknown
// predicates at ingest so the engine only ever sees vocabulary it can reason
// about in later phases.

#include <string>
#include <vector>

namespace verity::model {

/// The frozen starter predicate set.
[[nodiscard]] inline const std::vector<const char*>& knownPredicates() noexcept {
    static const std::vector<const char*> kPredicates = {
        "announced", "launched", "has_ceo", "located_at",
        "present_at", "happened_before", "cites",
    };
    return kPredicates;
}

[[nodiscard]] inline bool isKnownPredicate(const std::string& p) noexcept {
    for (const char* k : knownPredicates()) {
        if (p == k) return true;
    }
    return false;
}

} // namespace verity::model
