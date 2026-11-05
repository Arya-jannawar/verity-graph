#pragma once
// VERITY GRAPH — engine/TimelineBuilder: events → partial order → Hasse
// diagram / topological order (Phase 2).
//
// Semantics (frozen):
//   * `happened_before` claims build a strict order over events; dates add
//     implicit comparability ONLY when both dates are precise enough to be
//     disjoint (day/month precision, non-overlapping). Year-precision events
//     of the same year stay incomparable through dates alone.
//   * The combined relation must be a strict partial order. A cycle yields
//     finding kind "Potential temporal contradiction" WITH a witness path and
//     NO Hasse/topological output (withheld, not rendered as if valid).
//   * The Hasse diagram is the transitive reduction (dm::Relation).
//   * The AVL date index (ds::AVLTree over day-serials) answers
//     between(from, to) queries for the timeline view.
//
// Complexity: build O(claims + events); order checks O(n^3) (tiny n);
// Hasse O(n^3); topo O(n + e).

#include <optional>
#include <string>
#include <vector>

#include "verity/ds/AVLTree.hpp"
#include "verity/ds/TopoSort.hpp"
#include "verity/dm/Relations.hpp"
#include "verity/model/Extraction.hpp"

namespace verity::engine {

struct TimelineFinding {
    std::string statement;                 // always contains "Potential temporal contradiction"
    std::vector<std::string> witness;      // event ids forming the cycle (start == end)
    std::vector<std::string> claimIds;     // happened_before claims on the cycle
};

struct TimelineResult {
    /// events sorted by (start date serial, id) — chronological view.
    std::vector<std::string> chronologicalOrder;
    /// topological order of the strict order (empty when cyclic).
    std::vector<std::string> topologicalOrder;
    /// Hasse cover edges (a,b) meaning a strictly precedes b (empty when cyclic).
    std::vector<std::pair<std::string, std::string>> hasseEdges;
    /// DOT snippet of the Hasse diagram (empty when cyclic).
    std::string hasseDot;
    /// incomparable event pairs (a,b) with a<b lexicographic.
    std::vector<std::pair<std::string, std::string>> incomparablePairs;
    /// non-empty exactly when the order data is cyclic.
    std::optional<TimelineFinding> finding;
};

class TimelineBuilder {
public:
    [[nodiscard]] static TimelineResult build(const model::ExtractionRecord& records);

    /// Day serial for a DatePoint (proleptic Gregorian days from 0001-01-01).
    [[nodiscard]] static long long daySerial(const model::DatePoint& p);
};

} // namespace verity::engine
