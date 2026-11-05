#pragma once
// VERITY GRAPH — engine/ConflictRule: the Strategy hierarchy (OOP Unit 4).
//
// Each rule inspects the claim index and emits findings via the Observer.
// ADDING A RULE NEEDS ZERO CHANGES IN ConflictDetector: the detector holds a
// vector<unique_ptr<ConflictRule>> and iterates (tested in test_conflicts).
//
// RuleFactory::fromText(line) parses the rules.txt DSL against a frozen
// grammar; unknown lines throw RuleError (never eval/exec).
//
// Detection context (what rules may read):
//   * claims indexed by (subject, predicate) through the handwritten
//     ds::HashTable — the real engine path for the Phase-1 structure;
//   * the resolved canonical mapping (EntityResolver output);
//   * the TimelineBuilder result (for TemporalCycleRule).

#include <map>
#include <string>
#include <vector>

#include "verity/engine/Conflict.hpp"
#include "verity/engine/EntityResolver.hpp"
#include "verity/engine/FixedCache.hpp"
#include "verity/engine/TimelineBuilder.hpp"
#include "verity/model/Extraction.hpp"

namespace verity::engine {

struct DetectionContext {
    const model::ExtractionRecord* records = nullptr;
    const ResolutionResult* resolution = nullptr;
    const TimelineResult* timeline = nullptr;
    /// (subjectId, predicate) -> claim ids, from the handwritten HashTable.
    std::map<std::pair<std::string, std::string>, std::vector<std::string>> claimIndex;
    /// claim id -> record.
    std::map<std::string, const model::ClaimRecord*> claimById;

    /// Entity-lookup cache (FixedCache<K,V,N> with a non-type parameter —
    /// the Phase-3 template requirement, used on the real detection path).
    mutable FixedCache<std::string, std::string, 64> canonicalCache;

    [[nodiscard]] std::string canonicalOf(const std::string& entityId) const {
        if (const auto cached = canonicalCache.get(entityId)) return *cached;
        const std::string canonical = EntityResolver::canonicalOf(*resolution, entityId);
        canonicalCache.put(entityId, canonical);
        return canonical;
    }
};

class ConflictRule {
public:
    explicit ConflictRule(std::string name) : name_(std::move(name)) {}
    virtual ~ConflictRule() noexcept = default;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }

    /// Inspects the context and pushes findings into `findings`.
    virtual void detect(const DetectionContext& context,
                        std::vector<Conflict>& findings) const = 0;

protected:
    /// Counts and assigns finding ids deterministically (X001, X002, ...).
    static std::string nextFindingId(std::size_t& counter) {
        std::string id = "X" + std::to_string(counter + 1);
        while (id.size() < 4) id.insert(1, "0");
        ++counter;
        return id;
    }

private:
    std::string name_;
};

} // namespace verity::engine
