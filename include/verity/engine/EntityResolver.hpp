#pragma once
// VERITY GRAPH — engine/EntityResolver: mention entities → canonical entities
// (Phase 2).
//
// Algorithm (documented, deterministic):
//   1. BLOCKING: index entity names/aliases in the handwritten ds::Trie;
//      candidate pairs share a surname-like token (last whitespace token,
//      case-insensitive) — this bounds comparisons without merging unrelated
//      entities.
//   2. SIMILARITY: a pair matches when
//        (a) exact alias match: one's name/alias equals the other's
//            name/alias (case-insensitive), OR
//        (b) compatible initials: same surname (case-insensitive) AND the
//            first names are compatible — identical, or one is an initial
//            ("J.") matching the other's first initial.
//      An initial-only match does NOT merge when first names are known and
//      different (two different "A. Smith" people stay separate), and
//      different surnames never merge regardless of string similarity —
//      preventing unrelated common-surname merges.
//   3. RELATION: matched pairs populate a dm::Relation over entity ids;
//      the equivalence closure yields canonical classes.
//   4. CROSS-CHECK: ds::DisjointSet unions the same pairs; classes must agree
//      (test assertion, §Phase-2 acceptance).
//   5. CANONICAL: the representative of a class is the entity whose name is
//      NOT an abbreviated/alias form (longest name; ties → smallest id), so
//      "John Smith" wins over "J. Smith".
//
// Complexity: O(E log E) blocking + O(pair comparisons · distance) +
// O(n^3) closure (tiny n) + O(n α(n)) DSU cross-check.

#include <string>
#include <vector>

#include "verity/dm/Relations.hpp"
#include "verity/engine/GraphBuilder.hpp"
#include "verity/model/Extraction.hpp"

namespace verity::engine {

struct ResolutionResult {
    /// entity id -> canonical entity id (identity for singletons).
    std::vector<std::pair<std::string, std::string>> mapping;
    /// equivalence classes over entity ids (sorted, deterministic).
    std::vector<std::vector<std::string>> classes;
    /// merged alias book: canonical id -> union of all names/aliases in class.
    std::vector<std::pair<std::string, std::vector<std::string>>> aliasesByCanonical;
    /// number of merge operations performed.
    std::size_t mergedPairs = 0;
};

class EntityResolver {
public:
    /// Resolves alias/mention entities to canonical entities.
    [[nodiscard]] static ResolutionResult resolve(const model::ExtractionRecord& records);

    /// Convenience: canonical id for an entity id (identity when unmerged).
    [[nodiscard]] static std::string canonicalOf(const ResolutionResult& result,
                                                 const std::string& entityId);
};

} // namespace verity::engine
