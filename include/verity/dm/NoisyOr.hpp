#pragma once
// VERITY GRAPH — dm/NoisyOr: noisy-OR confidence aggregation (DM Unit 3
// applied part, Phase 3).
//
// combine(a, b) = 1 - (1 - a)(1 - b) over [0,1] with identity 0.
//
// HONESTY CONTRACT (frozen): noisy-OR is a CONFIDENCE AGGREGATION HEURISTIC
// under stated independence assumptions — it is NOT a probability that a
// statement is true. The engine never presents its output as a truth
// probability; every consumer must label it "confidence".
//
// Shared-source deduplication: combineEvidence deduplicates by document id
// and flags circular/dependent evidence before aggregation (a citation cycle
// among the sources lowers the aggregate — see Ranker).
//
// Monoid facts verified in tests: ([0,1], combine, 0) is a commutative
// monoid (associativity within floating-point tolerance; commutativity
// exact); merge order never changes the result (test uses tolerance).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// noisy-OR combine of two confidences in [0,1].
[[nodiscard]] inline double noisyOrCombine(double a, double b) {
    if (a < 0.0 || a > 1.0 || b < 0.0 || b > 1.0) {
        throw util::ParseError("noisy-OR inputs must lie in [0,1]");
    }
    return 1.0 - (1.0 - a) * (1.0 - b);
}

/// Aggregates a list of confidences; order-independent (tests verify with
/// floating-point tolerance). Empty input yields 0 (the identity).
[[nodiscard]] inline double noisyOrAggregate(const std::vector<double>& confidences) {
    double agg = 0.0;
    for (const double c : confidences) {
        agg = noisyOrCombine(agg, c);
    }
    return agg;
}

/// A scored evidence source for deduplicating aggregation.
struct EvidenceSource {
    std::string documentId;
    double confidence = 0.0;
    bool inCircularGroup = false;   // flagged by ProvenanceTracer
};

/// Deduplicates by document id (a document counts ONCE no matter how many
/// claims cite it), penalizes circular-source groups by a fixed conservative
/// factor (each circular source contributes half its confidence), and
/// aggregates with noisy-OR.
[[nodiscard]] inline double noisyOrAggregateEvidence(std::vector<EvidenceSource> sources) {
    std::sort(sources.begin(), sources.end(),
              [](const EvidenceSource& a, const EvidenceSource& b) {
                  return a.documentId < b.documentId;
              });
    sources.erase(std::unique(sources.begin(), sources.end(),
                              [](const EvidenceSource& a, const EvidenceSource& b) {
                                  return a.documentId == b.documentId;
                              }),
                  sources.end());
    std::vector<double> confidences;
    confidences.reserve(sources.size());
    for (const EvidenceSource& s : sources) {
        confidences.push_back(s.inCircularGroup ? s.confidence * 0.5 : s.confidence);
    }
    return noisyOrAggregate(confidences);
}

} // namespace verity::dm
