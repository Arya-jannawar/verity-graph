#include "verity/engine/VerityEngine.hpp"

#include <algorithm>
#include <cctype>

#include "verity/io/JsonLoader.hpp"
#include "verity/model/ClaimNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::engine {

void VerityEngine::ingest(const model::ExtractionRecord& records) {
    records_ = records;
    GraphBuilder::build(records_, store_, graph_);
}

VerityEngine VerityEngine::fromExtractionFile(const std::string& path) {
    VerityEngine engine;
    engine.ingest(io::JsonLoader::loadExtractionFile(path));
    return engine;
}

std::vector<std::pair<model::NodeId, int>> VerityEngine::neighbors(
    const model::NodeId& id, int depth) const {
    return ds::bfsKHop(graph_, id, depth);
}

std::vector<VerityEngine::EvidenceHit> VerityEngine::queryEvidence(
    const std::string& name) const {
    auto lower = [](const std::string& s) {
        std::string out(s);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    };
    const std::string needle = lower(name);

    std::vector<model::NodeId> matched;
    for (const auto* node : store_.all()) {
        const auto* entity = dynamic_cast<const model::Entity*>(node);
        if (entity == nullptr) continue;
        if (lower(entity->label()) == needle) {
            matched.push_back(entity->id());
            continue;
        }
        for (const std::string& alias : entity->aliases()) {
            if (lower(alias) == needle) {
                matched.push_back(entity->id());
                break;
            }
        }
    }
    // deterministic order
    std::sort(matched.begin(), matched.end());

    std::vector<EvidenceHit> hits;
    for (const auto& claimRec : records_.claims) {
        const model::NodeId subjectId(claimRec.subject_id);
        if (std::find(matched.begin(), matched.end(), subjectId) == matched.end()) {
            continue;
        }
        EvidenceHit hit{
            subjectId,
            model::NodeId(claimRec.id),
            claimRec.predicate,
            model::ClaimNode::objectToString(claimRec.object),
            model::NodeId(claimRec.document_id),
            claimRec.span.quote,
            claimRec.confidence,
        };
        hits.push_back(std::move(hit));
    }
    return hits;
}

} // namespace verity::engine
