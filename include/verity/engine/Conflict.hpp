#pragma once
// VERITY GRAPH — engine/Conflict: a finding produced by the reasoning engine.
//
// Wording contract (frozen, docs/CONTRACT.md §10):
//   * statement ALWAYS contains the exact phrase "Potential contradiction"
//     (temporal findings: "Potential temporal contradiction").
//   * NO red verdict badges: severity is an analysis label, never a truth
//     verdict, lie-detector output, or probability of truth.
//   * every finding carries a possible benign explanation and keeps both
//     claims visible with their documents and exact excerpt quotes.

#include <string>
#include <vector>

#include "verity/model/Extraction.hpp"

namespace verity::engine {

struct Conflict {
    std::string id;                       // X001, X002, ...
    std::string rule;                     // rule name (strategy that found it)
    std::string statement;                // contains "Potential contradiction"
    std::string claimA;                   // claim id
    std::string claimB;                   // claim id ("" for some rule types)
    std::string subjectId;                // canonical subject entity (or event)
    std::string predicate;
    std::vector<std::string> documentIds; // all involved documents
    struct Excerpt {
        std::string documentId;
        std::size_t start = 0;
        std::size_t end = 0;
        std::string quote;
    };
    std::vector<Excerpt> excerpts;        // exact quotes from the sources
    std::vector<std::string> derivation;  // ordered reasoning steps
    std::string severity;                 // "low" | "medium" | "high"
    double confidence = 0.0;              // combined extraction confidence
    std::string benignExplanation;        // why it may NOT be a real contradiction
};

/// Observer interface (OOP Unit 4): ConflictDetector notifies subscribers.
class ConflictListener {
public:
    virtual ~ConflictListener() noexcept = default;
    virtual void onFinding(const Conflict& finding) = 0;
};

/// CLI listener: prints findings as they are detected.
class ConflictCliPrinter final : public ConflictListener {
public:
    void onFinding(const Conflict& finding) override;
};

/// Collector listener: accumulates findings for structured output.
class ConflictJsonCollector final : public ConflictListener {
public:
    void onFinding(const Conflict& finding) override;
    [[nodiscard]] const std::vector<Conflict>& findings() const noexcept { return findings_; }

private:
    std::vector<Conflict> findings_;
};

} // namespace verity::engine
