#include "verity/engine/TimelineBuilder.hpp"

#include <algorithm>
#include <map>
#include <set>

#include "verity/util/VerityError.hpp"

namespace verity::engine {

long long TimelineBuilder::daySerial(const model::DatePoint& p) {
    // days from a fixed epoch using the standard civil-days algorithm
    long long y = p.year;
    const long long m = p.month ? p.month : 1;
    const long long d = p.day ? p.day : 1;
    y -= m <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const long long yoe = y - era * 400;
    const long long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe;
}

TimelineResult TimelineBuilder::build(const model::ExtractionRecord& records) {
    TimelineResult result;

    // event lookup
    std::set<std::string> eventIds;
    std::map<std::string, const model::EventRecord*> events;
    for (const auto& ev : records.events) {
        eventIds.insert(ev.id);
        events[ev.id] = &ev;
    }

    // --- date index (AVL over day serials) ----------------------------------
    ds::AVLTree<std::pair<long long, std::string>> dateIndex;   // (serial, id) unique
    for (const auto& ev : records.events) {
        const model::DateInterval interval = model::DateInterval::fromString(ev.date);
        dateIndex.insert({daySerial(interval.start), ev.id});
    }

    // chronological order: by start serial then id
    result.chronologicalOrder.reserve(records.events.size());
    for (const auto& key : dateIndex.inOrder()) result.chronologicalOrder.push_back(key.second);

    // --- strict order relation over events -----------------------------------
    dm::Relation<std::string> order(std::vector<std::string>(eventIds.begin(), eventIds.end()));

    // date-derived comparability: precise, disjoint intervals imply order
    for (const auto& a : records.events) {
        const model::DateInterval ia = model::DateInterval::fromString(a.date);
        for (const auto& b : records.events) {
            if (a.id == b.id) continue;
            const model::DateInterval ib = model::DateInterval::fromString(b.date);
            const bool preciseEnough =
                ia.precision != model::DatePrecision::Year ||
                ib.precision != model::DatePrecision::Year;
            if (preciseEnough && ia.strictlyBefore(ib)) order.add(a.id, b.id);
        }
    }

    // happened_before claims (subject and object must both be events)
    std::map<std::pair<std::string, std::string>, std::string> claimForEdge;
    for (const auto& claim : records.claims) {
        if (claim.predicate != "happened_before") continue;
        if (!eventIds.count(claim.subject_id) || claim.object.kind != model::ClaimObject::Kind::EntityRef ||
            !eventIds.count(claim.object.entity_id)) {
            continue;
        }
        order.add(claim.subject_id, claim.object.entity_id);
        claimForEdge[{claim.subject_id, claim.object.entity_id}] = claim.id;
    }

    // --- cycle check ---------------------------------------------------------
    if (!order.isStrictOrder()) {
        // a cycle breaks asymmetry/transitivity: build a graph and find a witness
        ds::Graph<std::string, int> graph(true);
        for (const auto& id : eventIds) graph.addNode(id);
        for (const auto& a : records.events) {
            const model::DateInterval ia = model::DateInterval::fromString(a.date);
            for (const auto& b : records.events) {
                if (a.id == b.id) continue;
                const model::DateInterval ib = model::DateInterval::fromString(b.date);
                const bool preciseEnough = ia.precision != model::DatePrecision::Year ||
                                           ib.precision != model::DatePrecision::Year;
                if (preciseEnough && ia.strictlyBefore(ib)) graph.addEdge(a.id, b.id, 1);
            }
        }
        std::set<std::string> cycleClaimIds;
        std::vector<std::pair<std::string, std::string>> claimEdges;
        for (const auto& claim : records.claims) {
            if (claim.predicate == "happened_before" && eventIds.count(claim.subject_id) &&
                claim.object.kind == model::ClaimObject::Kind::EntityRef &&
                eventIds.count(claim.object.entity_id)) {
                graph.addEdge(claim.subject_id, claim.object.entity_id, 1);
                claimEdges.emplace_back(claim.subject_id, claim.object.entity_id);
            }
        }
        TimelineFinding finding;
        finding.statement = "Potential temporal contradiction";
        finding.witness = ds::directedCycle(graph);
        for (std::size_t i = 0; i + 1 < finding.witness.size(); ++i) {
            const auto it = claimForEdge.find({finding.witness[i], finding.witness[i + 1]});
            if (it != claimForEdge.end()) cycleClaimIds.insert(it->second);
        }
        finding.claimIds.assign(cycleClaimIds.begin(), cycleClaimIds.end());
        result.finding = std::move(finding);
        return result;
    }

    // --- Hasse diagram + topological order -----------------------------------
    const dm::Relation<std::string> hasse = order.hasseDiagram();
    for (const auto& a : hasse.elements()) {
        for (const auto& b : hasse.elements()) {
            if (a < b && hasse.contains(a, b)) result.hasseEdges.emplace_back(a, b);
        }
    }

    ds::Graph<std::string, int> dag(true);
    for (const auto& id : eventIds) dag.addNode(id);
    for (const auto& [a, b] : result.hasseEdges) dag.addEdge(a, b, 1);
    result.topologicalOrder = ds::kahnTopoSort(dag);

    // DOT output of the Hasse diagram
    result.hasseDot = "digraph hasse {\n  rankdir=BT;\n";
    for (const auto& [a, b] : result.hasseEdges) {
        result.hasseDot += "  \"" + a + "\" -> \"" + b + "\";\n";
    }
    result.hasseDot += "}\n";

    result.incomparablePairs = order.incomparablePairs();
    return result;
}

} // namespace verity::engine
