#pragma once
// VERITY GRAPH — engine::GraphBuilder: extraction records -> typed graph.
//
// Builds the ds::Graph<NodeId, EdgeAttr> evidence graph from validated
// extraction records, creating model nodes in a NodeStore (which owns them).
//
// Edge creation rules (frozen):
//   claim --SubjectOf--> subject entity
//   claim --ObjectOf---> object entity       (EntityRef objects only)
//   claim --DocumentEvidence--> document
//   entity --MentionedIn--> document         (one per mention span)
//   event --SubjectOf---> entity
//   event --DocumentEvidence--> document
//   document --Cites---> document            (citations, incl. root-level)
//
// Complexity: O(D + N + C + EV + total edges).

#include <string>

#include "verity/ds/Graph.hpp"
#include "verity/model/Enums.hpp"
#include "verity/model/Extraction.hpp"
#include "verity/model/NodeStore.hpp"

namespace verity::engine {

/// Edge payload of the evidence graph.
struct EdgeAttr {
    model::RelationType relation;
    double weight = 1.0;      // normalized in [0,1]; phase 3 uses 1-confidence
    std::string label;        // human-readable, e.g. the predicate
};

using EvidenceGraph = ds::Graph<model::NodeId, EdgeAttr>;

class GraphBuilder {
public:
    /// Builds nodes + edges. `store` takes ownership of all created nodes.
    /// Returns the number of graph edges created.
    static std::size_t build(const model::ExtractionRecord& records,
                             model::NodeStore& store, EvidenceGraph& graph);
};

} // namespace verity::engine
