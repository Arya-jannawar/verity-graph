#pragma once
// VERITY GRAPH — VerityEngine: facade over the evidence graph.
//
// Ingestion + queries. Phase 1: neighbors (BFS k-hop via ds::GraphAlgo) and
// evidence lookup. Later phases add resolvers, timeline, conflicts, ranking,
// provenance and exporters behind the same facade.
//
// Complexity: ingest O(records + edges); neighbors O(V+E); evidence query
// O(entities + their claims).

#include <string>
#include <utility>
#include <vector>

#include "verity/ds/GraphAlgo.hpp"
#include "verity/engine/GraphBuilder.hpp"
#include "verity/model/Extraction.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/model/NodeStore.hpp"

namespace verity::engine {

class VerityEngine {
public:
    struct EvidenceHit {
        model::NodeId entity;
        model::NodeId claim;
        std::string predicate;
        std::string objectText;
        model::NodeId document;
        std::string quote;      // exact claim span quote
        double confidence = 0.0;
    };

    VerityEngine() = default;

    /// Ingest validated extraction records: builds the NodeStore + graph.
    /// Throws util::VerityError on duplicate/missing ids (data bugs).
    void ingest(const model::ExtractionRecord& records);

    /// Convenience: load + ingest from an extraction JSON file (io::JsonLoader).
    static VerityEngine fromExtractionFile(const std::string& path);

    [[nodiscard]] const model::NodeStore& store() const noexcept { return store_; }
    [[nodiscard]] const EvidenceGraph& graph() const noexcept { return graph_; }
    [[nodiscard]] const model::ExtractionRecord& records() const noexcept { return records_; }

    /// BFS neighborhood within `depth` hops: {id, distance} in discovery order.
    [[nodiscard]] std::vector<std::pair<model::NodeId, int>> neighbors(
        const model::NodeId& id, int depth) const;

    /// Claims whose subject is a canonical entity matching `name` exactly,
    /// case-insensitively, or through an alias. Deterministic order (claim id).
    [[nodiscard]] std::vector<EvidenceHit> queryEvidence(const std::string& name) const;

private:
    model::NodeStore store_;
    EvidenceGraph graph_{true};
    model::ExtractionRecord records_;
};

} // namespace verity::engine
