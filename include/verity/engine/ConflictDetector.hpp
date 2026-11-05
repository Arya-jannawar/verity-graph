#pragma once
// VERITY GRAPH — engine/ConflictDetector: Strategy-based contradiction
// detection with Observer notification (OOP Unit 4, Phase 3).
//
// FROZEN SEMANTICS (docs/CONTRACT.md §10):
//   1. compare compatible predicates only, canonical subjects, same units;
//   2. month-vs-day overlap is NOT a contradiction (interval overlap);
//   3. different has_ceo values at DISJOINT times are consistent;
//   4. unknown polarity is preserved — unknown ≠ negation;
//   5. severity is separate from extraction confidence;
//   6. temporal cycles report a witness and withhold Hasse output;
//   7. findings always say "Potential contradiction".
//
// Rules live in ConflictRule subclasses; RuleFactory::fromText parses
// data/rules/rules.txt against a frozen grammar (RuleError on violations).
// Adding a rule requires NO change in this class (see test_conflicts).

#include <memory>
#include <vector>

#include "verity/ds/HashTable.hpp"
#include "verity/engine/Conflict.hpp"
#include "verity/engine/ConflictRule.hpp"
#include "verity/util/Logger.hpp"

namespace verity::engine {

class ConflictDetector {
public:
    ConflictDetector();

    /// Registers a strategy. The detector takes ownership.
    void addRule(std::unique_ptr<ConflictRule> rule);

    /// Subscribes an Observer (not owned; must outlive the detector).
    void addListener(ConflictListener* listener);

    /// Runs every rule in registration order, sorts findings deterministically
    /// by (rule, claimA, claimB), assigns ids X001..., notifies listeners.
    [[nodiscard]] std::vector<Conflict> run(const DetectionContext& context) const;

    /// Builds the detection context: claims indexed by (subject, predicate)
    /// through the handwritten ds::HashTable; canonicalization via
    /// FixedCache<string, string, 64> (entity lookups).
    [[nodiscard]] static DetectionContext buildContext(const model::ExtractionRecord& records,
                                                       const ResolutionResult& resolution,
                                                       const TimelineResult& timeline);

    /// RuleFactory: parses one rules.txt line into a rule instance.
    class RuleFactory {
    public:
        /// Grammar:
        ///   "#" comment | blank
        ///   functional <predicate> overlap
        ///   negation <predicate|any> overlap
        ///   numeric <predicate|any> same_unit
        ///   location <predicate> overlap
        ///   temporal_cycle <predicate>
        /// Throws RuleError on any deviation.
        [[nodiscard]] static std::unique_ptr<ConflictRule> fromText(const std::string& line);
    };

private:
    std::vector<std::unique_ptr<ConflictRule>> rules_;
    std::vector<ConflictListener*> listeners_;
};

} // namespace verity::engine
