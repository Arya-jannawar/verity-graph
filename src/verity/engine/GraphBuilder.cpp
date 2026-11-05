#include "verity/engine/GraphBuilder.hpp"

#include <string>

#include "verity/model/ClaimNode.hpp"
#include "verity/model/DocumentNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/EventNode.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::engine {

std::size_t GraphBuilder::build(const model::ExtractionRecord& records,
                                model::NodeStore& store, EvidenceGraph& graph) {
    std::size_t edges = 0;

    // Feed every loaded id into the counters so post-load nextId() calls
    // never collide with ids from the document set.
    for (const auto& d : records.documents) model::Node::observeId(d.id);
    for (const auto& e : records.entities) model::Node::observeId(e.id);
    for (const auto& c : records.claims) model::Node::observeId(c.id);
    for (const auto& ev : records.events) model::Node::observeId(ev.id);

    // --- documents ----------------------------------------------------------
    for (const auto& d : records.documents) {
        auto node = std::make_unique<model::DocumentNode>(
            d.filename, model::documentFormatFromString(d.format), d.normalized_text,
            d.citations, d.id);
        graph.addNode(node->id());
        (void)store.add(std::move(node));
    }

    // --- entities -----------------------------------------------------------
    for (const auto& e : records.entities) {
        auto node = std::make_unique<model::Entity>(e.name,
                                                    model::entityTypeFromString(e.type),
                                                    e.aliases, e.id);
        const model::NodeId entityId = node->id();   // copy BEFORE ownership moves
        graph.addNode(entityId);
        (void)store.add(std::move(node));
        for (const auto& m : e.mentions) {
            EdgeAttr attr;
            attr.relation = model::RelationType::MentionedIn;
            attr.label = "mention";
            graph.addEdge(entityId, model::NodeId(m.document_id), attr);
            ++edges;
        }
    }

    // --- claims -------------------------------------------------------------
    for (const auto& c : records.claims) {
        const model::NodeId subjectId(c.subject_id);
        const model::NodeId docId(c.document_id);
        auto node = std::make_unique<model::ClaimNode>(
            c.predicate + " (" + c.id + ")", subjectId, c.predicate, c.object, docId,
            c.span, model::polarityFromString(c.polarity), c.confidence, c.id);
        if (c.valid_from || c.valid_to) {
            // Valid-time interval: prefer explicit from/to; single bound is a
            // point interval on that bound.
            if (c.valid_from && c.valid_to) {
                nlohmann::json range = {{"from", *c.valid_from}, {"to", *c.valid_to}};
                node->setValidTime(model::DateInterval::fromJson(range));
            } else if (c.valid_from) {
                node->setValidTime(model::DateInterval::fromString(*c.valid_from));
            } else {
                node->setValidTime(model::DateInterval::fromString(*c.valid_to));
            }
        }
        // Everything that reads `node` happens BEFORE ownership is transferred.
        const model::NodeId claimId = node->id();
        const double confidence = node->confidence();
        graph.addNode(claimId);

        EdgeAttr subjectEdge;
        subjectEdge.relation = model::RelationType::SubjectOf;
        subjectEdge.weight = confidence;
        subjectEdge.label = c.predicate;
        graph.addEdge(claimId, subjectId, subjectEdge);
        ++edges;

        if (c.object.kind == model::ClaimObject::Kind::EntityRef) {
            EdgeAttr objectEdge;
            objectEdge.relation = model::RelationType::ObjectOf;
            objectEdge.weight = confidence;
            objectEdge.label = c.predicate;
            graph.addEdge(claimId, model::NodeId(c.object.entity_id), objectEdge);
            ++edges;
        }

        EdgeAttr evidenceEdge;
        evidenceEdge.relation = model::RelationType::DocumentEvidence;
        evidenceEdge.weight = confidence;
        evidenceEdge.label = "evidence";
        graph.addEdge(claimId, docId, evidenceEdge);
        ++edges;

        (void)store.add(std::move(node));
    }

    // --- events -------------------------------------------------------------
    for (const auto& ev : records.events) {
        const model::NodeId entityId(ev.entity_id);
        const model::NodeId docId(ev.document_id);
        auto node = std::make_unique<model::EventNode>(ev.label, entityId,
                                                       model::DateInterval::fromString(ev.date),
                                                       docId, ev.span, ev.id);
        const model::NodeId eventId = node->id();
        graph.addNode(eventId);

        EdgeAttr subjectEdge;
        subjectEdge.relation = model::RelationType::SubjectOf;
        subjectEdge.label = "event_subject";
        graph.addEdge(eventId, entityId, subjectEdge);
        ++edges;

        EdgeAttr evidenceEdge;
        evidenceEdge.relation = model::RelationType::DocumentEvidence;
        evidenceEdge.label = "evidence";
        graph.addEdge(eventId, docId, evidenceEdge);
        ++edges;

        (void)store.add(std::move(node));
    }

    // --- root-level citation records ----------------------------------------
    for (const auto& ct : records.citations) {
        EdgeAttr attr;
        attr.relation = model::RelationType::Cites;
        attr.label = "cites";
        graph.addEdge(model::NodeId(ct.citing), model::NodeId(ct.cited), attr);
        ++edges;
    }

    return edges;
}

} // namespace verity::engine
