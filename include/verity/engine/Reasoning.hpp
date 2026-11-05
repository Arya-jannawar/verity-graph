#pragma once
// VERITY GRAPH — engine/Reasoning: EvidenceBuilder, ProvenanceTracer, Ranker
// (Phase 3).
//
// EvidenceBuilder links claims to documents through the three Evidence
// SIBLINGS (Supporting / Contradicting / Neutral, Phase 2 family):
//   * SupportingEvidence(claim, its own document, confidence)
//   * ContradictingEvidence(claim, the OTHER claim's document, per conflict)
//   * NeutralEvidence(claim, documents that mention the subject without
//     supporting or contradicting it)
//
// ProvenanceTracer walks the citation graph (Cites edges):
//   * origin tree per claim: claim -> its document -> documents it cites
//     (BFS spanning tree; non-tree/cyclic edges RETAINED so the UI never
//     implies the full provenance graph is a tree);
//   * circular sourcing groups via directed cycle detection with witness;
//   * repeated citations vs independent support are DISTINGUISHED.
//
// Ranker scores entities/evidence:
//   * key intermediaries: degree in the evidence graph + noisy-OR of
//     incident claim confidences (circular-group halved, deduped);
//   * top-K evidence via ds::topK over IScorable scores;
//   * MST backbone via Kruskal on the undirected projection with weight
//     1 - confidence (minimizes that defined additive cost — explained in
//     output, not a generic reliability score).

#include <string>
#include <vector>

#include "verity/ds/GraphAlgorithms.hpp"
#include "verity/engine/Conflict.hpp"
#include "verity/engine/GraphBuilder.hpp"
#include "verity/model/Evidence.hpp"

namespace verity::engine {

struct EvidenceBundle {
    std::vector<std::unique_ptr<model::Evidence>> all;   // owned, in build order
    std::vector<model::Evidence*> supporting;
    std::vector<model::Evidence*> contradicting;
    std::vector<model::Evidence*> neutral;
};

class EvidenceBuilder {
public:
    /// Builds the evidence bundle for the claim set. `conflicts` drive the
    /// Contradicting links. Document texts come from the records (mentions
    /// drive Neutral links).
    [[nodiscard]] static EvidenceBundle build(const model::ExtractionRecord& records,
                                              const std::vector<Conflict>& conflicts);
};

struct OriginTree {
    std::string claimId;
    std::string rootDocument;                 // the claim's own document
    std::vector<std::pair<std::string, std::string>> treeEdges;   // BFS tree
    std::vector<std::pair<std::string, std::string>> extraEdges;  // non-tree (incl. cyclic)
    bool reachableWithoutCycle = true;
};

struct CircularGroup {
    std::vector<std::string> witness;         // document ids, start == end
    std::vector<std::pair<std::string, std::string>> edges;
};

class ProvenanceTracer {
public:
    [[nodiscard]] static OriginTree originTreeOf(const model::ExtractionRecord& records,
                                                 const std::string& claimId);
    /// All document citation cycles (usually the planted D006->D007->D008->D006).
    [[nodiscard]] static std::vector<CircularGroup> circularGroups(const model::ExtractionRecord& records);
    /// Claims in circular-support groups are flagged (Ranker halves them).
    [[nodiscard]] static bool claimInCircularGroup(const model::ExtractionRecord& records,
                                                   const std::string& claimId);
};

struct KeyEntityScore {
    std::string entityId;
    std::string label;
    std::size_t degree = 0;
    double evidenceScore = 0.0;   // noisy-OR (deduped, circular-halved)
};

class Ranker {
public:
    /// Key entities: top-K by degree, scored with noisy-OR of incident claim
    /// confidences. Uses FixedCache-style dedup via document ids.
    [[nodiscard]] static std::vector<KeyEntityScore> keyEntities(
        const model::ExtractionRecord& records, const model::NodeStore& store,
        const EvidenceGraph& graph, std::size_t k);

    /// Top-K evidence objects by IScorable score.
    [[nodiscard]] static std::vector<const model::IScorable*> topEvidence(
        const std::vector<const model::IScorable*>& candidates, std::size_t k);

    /// MST backbone (Kruskal on the undirected projection, weight = 1 -
    /// confidence; explains the minimized additive cost).
    [[nodiscard]] static std::vector<std::tuple<std::string, std::string, double>> backbone(
        const EvidenceGraph& graph);
};

} // namespace verity::engine
