#include "verity/engine/Reasoning.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "verity/ds/Queue.hpp"
#include "verity/ds/TopoSort.hpp"
#include "verity/engine/EntityResolver.hpp"
#include "verity/dm/NoisyOr.hpp"

namespace verity::engine {

// --- EvidenceBuilder ---------------------------------------------------------

EvidenceBundle EvidenceBuilder::build(const model::ExtractionRecord& records,
                                      const std::vector<Conflict>& conflicts) {
    EvidenceBundle bundle;

    // claim -> its own document (supporting by construction)
    for (const auto& claim : records.claims) {
        auto evidence = std::make_unique<model::SupportingEvidence>(
            model::NodeId(claim.id), model::NodeId(claim.document_id), claim.confidence);
        bundle.supporting.push_back(evidence.get());
        bundle.all.push_back(std::move(evidence));
    }

    // conflicts produce contradicting links: claim A is contradicted by
    // claim B's document and vice versa
    for (const Conflict& conflict : conflicts) {
        if (conflict.claimA.empty() || conflict.claimB.empty()) continue;
        const auto itA = std::find_if(records.claims.begin(), records.claims.end(),
                                      [&](const model::ClaimRecord& c) {
                                          return c.id == conflict.claimA;
                                      });
        const auto itB = std::find_if(records.claims.begin(), records.claims.end(),
                                      [&](const model::ClaimRecord& c) {
                                          return c.id == conflict.claimB;
                                      });
        if (itA == records.claims.end() || itB == records.claims.end()) continue;
        const double strength = conflict.severity == "high"   ? 0.9
                                : conflict.severity == "medium" ? 0.7
                                                                : 0.5;
        auto ab = std::make_unique<model::ContradictingEvidence>(
            model::NodeId(conflict.claimA), model::NodeId(itB->document_id), strength);
        bundle.contradicting.push_back(ab.get());
        bundle.all.push_back(std::move(ab));
        auto ba = std::make_unique<model::ContradictingEvidence>(
            model::NodeId(conflict.claimB), model::NodeId(itA->document_id), strength);
        bundle.contradicting.push_back(ba.get());
        bundle.all.push_back(std::move(ba));
    }

    // neutral links: documents mentioning the subject but not the claim's own
    // document nor a contradicting one
    std::set<std::string> contradictingDocs;
    for (const auto* evidence : bundle.contradicting) {
        contradictingDocs.insert(evidence->documentId().str());
    }
    for (const auto& entity : records.entities) {
        for (const auto& mention : entity.mentions) {
            for (const auto& claim : records.claims) {
                if (claim.subject_id != entity.id) continue;
                if (claim.document_id == mention.document_id) continue;   // supporting
                if (contradictingDocs.count(mention.document_id) != 0u) continue;
                auto evidence = std::make_unique<model::NeutralEvidence>(
                    model::NodeId(claim.id), model::NodeId(mention.document_id), 0.3);
                bundle.neutral.push_back(evidence.get());
                bundle.all.push_back(std::move(evidence));
            }
        }
    }
    return bundle;
}

// --- ProvenanceTracer ----------------------------------------------------------

namespace {

std::vector<std::pair<std::string, std::string>> citationEdges(const model::ExtractionRecord& records) {
    std::set<std::pair<std::string, std::string>> edges;
    for (const auto& doc : records.documents) {
        for (const std::string& cited : doc.citations) {
            edges.emplace(doc.id, cited);
        }
    }
    for (const auto& citation : records.citations) {
        edges.emplace(citation.citing, citation.cited);
    }
    return std::vector<std::pair<std::string, std::string>>(edges.begin(), edges.end());
}

} // namespace

OriginTree ProvenanceTracer::originTreeOf(const model::ExtractionRecord& records,
                                          const std::string& claimId) {
    OriginTree tree;
    tree.claimId = claimId;
    const auto claimIt = std::find_if(records.claims.begin(), records.claims.end(),
                                      [&](const model::ClaimRecord& c) { return c.id == claimId; });
    if (claimIt == records.claims.end()) {
        tree.rootDocument = "";
        return tree;
    }
    tree.rootDocument = claimIt->document_id;

    // BFS spanning tree over the citation graph from the root document;
    // every non-tree edge (including cyclic ones) is retained separately.
    const auto edges = citationEdges(records);
    ds::Graph<std::string, int> graph(true);
    std::set<std::string> docIds;
    for (const auto& doc : records.documents) {
        graph.addNode(doc.id);
        docIds.insert(doc.id);
    }
    for (const auto& [from, to] : edges) graph.addEdge(from, to, 1);

    std::set<std::string> visited{tree.rootDocument};
    ds::Queue<std::string> frontier;
    frontier.enqueue(tree.rootDocument);
    while (!frontier.empty()) {
        const std::string node = frontier.dequeue();
        for (const auto& [to, w] : graph.neighbors(node)) {
            (void)w;
            if (visited.count(to) != 0u) continue;
            visited.insert(to);
            tree.treeEdges.emplace_back(node, to);
            frontier.enqueue(to);
        }
    }
    // non-tree edges (cyclic/back edges stay visible)
    std::set<std::pair<std::string, std::string>> treeSet(tree.treeEdges.begin(),
                                                          tree.treeEdges.end());
    for (const auto& [from, to] : edges) {
        if (treeSet.count({from, to}) == 0u) tree.extraEdges.emplace_back(from, to);
    }
    // a cycle touching the origin root makes the origin "cyclic"
    const auto groups = circularGroups(records);
    for (const CircularGroup& group : groups) {
        if (std::find(group.witness.begin(), group.witness.end(), tree.rootDocument) !=
            group.witness.end()) {
            tree.reachableWithoutCycle = false;
        }
    }
    return tree;
}

std::vector<CircularGroup> ProvenanceTracer::circularGroups(const model::ExtractionRecord& records) {
    ds::Graph<std::string, int> graph(true);
    for (const auto& doc : records.documents) graph.addNode(doc.id);
    for (const auto& [from, to] : citationEdges(records)) graph.addEdge(from, to, 1);

    std::vector<CircularGroup> groups;
    // every cycle witness from every start (deduplicated by node set)
    std::set<std::vector<std::string>> seen;
    for (std::size_t round = 0; round < records.documents.size() + 1; ++round) {
        const auto witness = ds::directedCycle(graph);
        if (witness.empty()) break;
        const std::vector<std::string> canonical(witness.begin(), witness.end());
        if (seen.insert(canonical).second) {
            CircularGroup group;
            group.witness = witness;
            for (std::size_t i = 0; i + 1 < witness.size(); ++i) {
                group.edges.emplace_back(witness[i], witness[i + 1]);
            }
            groups.push_back(std::move(group));
        }
        // break the found cycle by removing one edge so further cycles surface
        if (witness.size() >= 2) {
            graph.removeEdge(witness[0], witness[1]);
        }
    }
    return groups;
}

bool ProvenanceTracer::claimInCircularGroup(const model::ExtractionRecord& records,
                                            const std::string& claimId) {
    const auto claimIt = std::find_if(records.claims.begin(), records.claims.end(),
                                      [&](const model::ClaimRecord& c) { return c.id == claimId; });
    if (claimIt == records.claims.end()) return false;
    const auto groups = circularGroups(records);
    for (const CircularGroup& group : groups) {
        if (std::find(group.witness.begin(), group.witness.end(), claimIt->document_id) !=
            group.witness.end()) {
            return true;
        }
    }
    return false;
}

// --- Ranker ----------------------------------------------------------------------

std::vector<KeyEntityScore> Ranker::keyEntities(const model::ExtractionRecord& records,
                                                const model::NodeStore& store,
                                                const EvidenceGraph& graph, std::size_t k) {
    const ResolutionResult resolution = EntityResolver::resolve(records);
    std::vector<KeyEntityScore> scores;
    for (const auto& entity : records.entities) {
        KeyEntityScore score;
        score.entityId = entity.id;
        score.label = entity.name;
        const auto* node = store.find(model::NodeId(entity.id));
        if (node != nullptr && graph.hasNode(model::NodeId(entity.id))) {
            score.degree = graph.degree(model::NodeId(entity.id));
        }
        // noisy-OR over incident claim confidences (deduped per document,
        // circular-group sources halved)
        std::vector<verity::dm::EvidenceSource> sources;
        for (const auto& claim : records.claims) {
            if (EntityResolver::canonicalOf(resolution, claim.subject_id) != entity.id) continue;
            verity::dm::EvidenceSource source;
            source.documentId = claim.document_id;
            source.confidence = claim.confidence;
            source.inCircularGroup = ProvenanceTracer::claimInCircularGroup(records, claim.id);
            sources.push_back(std::move(source));
        }
        score.evidenceScore = verity::dm::noisyOrAggregateEvidence(std::move(sources));
        scores.push_back(std::move(score));
    }
    std::sort(scores.begin(), scores.end(), [](const KeyEntityScore& a, const KeyEntityScore& b) {
        if (a.degree != b.degree) return a.degree > b.degree;
        if (a.evidenceScore != b.evidenceScore) return a.evidenceScore > b.evidenceScore;
        return a.entityId < b.entityId;
    });
    if (scores.size() > k) scores.resize(k);
    return scores;
}

std::vector<const model::IScorable*> Ranker::topEvidence(
    const std::vector<const model::IScorable*>& candidates, std::size_t k) {
    return ds::topK(candidates, k, [](const model::IScorable* a, const model::IScorable* b) {
        if (a == nullptr || b == nullptr) return a != nullptr;
        return a->score() > b->score();
    });
}

std::vector<std::tuple<std::string, std::string, double>> Ranker::backbone(
    const EvidenceGraph& graph) {
    // Kruskal over the undirected projection with the documented additive
    // cost 1 - confidence: the result MINIMIZES this cost, it is not a
    // generic reliability score.
    const auto mst = ds::kruskalMst(graph, [](const EdgeAttr& attr) { return 1.0 - attr.weight; });
    std::vector<std::tuple<std::string, std::string, double>> out;
    out.reserve(mst.size());
    for (const auto& [a, b, w] : mst) out.emplace_back(a.str(), b.str(), w);
    return out;
}

} // namespace verity::engine
