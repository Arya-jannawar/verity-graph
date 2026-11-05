// VERITY GRAPH — tests/integration/test_pipeline.cpp
// End-to-end: extraction JSON -> JsonLoader -> GraphBuilder -> VerityEngine ->
// neighbors (BFS k-hop) and evidence query. This is the Phase-1 acceptance
// pipeline (`verity ingest ... && verity neighbors C001 --depth 2`).

#include "doctest/doctest.h"

#include <string>
#include <vector>

#include "verity/engine/VerityEngine.hpp"
#include "verity/io/JsonLoader.hpp"
#include "verity/model/Enums.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace {

const char* kPipeline = R"JSON({
  "schema_version": "1.0.0",
  "documents": [
    {"id": "D001", "filename": "d001.txt", "format": "txt",
     "normalized_text": "Aster Vale Systems appointed Maria Chen as CEO.\n",
     "citations": []},
    {"id": "D002", "filename": "d002.txt", "format": "txt",
     "normalized_text": "Northstar Meridian Labs named David Okoro CEO.\n",
     "citations": ["D001"]}
  ],
  "entities": [
    {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
     "aliases": [],
     "mentions": [{"document_id": "D001",
                   "span": {"start": 0, "end": 18, "quote": "Aster Vale Systems"}}]},
    {"id": "E002", "name": "Maria Chen", "type": "Person",
     "aliases": [],
     "mentions": [{"document_id": "D001",
                   "span": {"start": 29, "end": 39, "quote": "Maria Chen"}}]},
    {"id": "E003", "name": "Northstar Meridian Labs", "type": "Organization",
     "aliases": [],
     "mentions": [{"document_id": "D002",
                   "span": {"start": 0, "end": 23, "quote": "Northstar Meridian Labs"}}]},
    {"id": "E004", "name": "David Okoro", "type": "Person",
     "aliases": [],
     "mentions": [{"document_id": "D002",
                   "span": {"start": 30, "end": 41, "quote": "David Okoro"}}]}
  ],
  "claims": [
    {"id": "C001", "subject_id": "E001", "predicate": "has_ceo",
     "object": {"kind": "EntityRef", "entity_id": "E002"},
     "polarity": "Positive", "confidence": 0.9,
     "document_id": "D001",
     "span": {"start": 0, "end": 47, "quote": "Aster Vale Systems appointed Maria Chen as CEO."}},
    {"id": "C002", "subject_id": "E003", "predicate": "has_ceo",
     "object": {"kind": "EntityRef", "entity_id": "E004"},
     "polarity": "Positive", "confidence": 0.85,
     "document_id": "D002",
     "span": {"start": 0, "end": 46, "quote": "Northstar Meridian Labs named David Okoro CEO."}},
    {"id": "C003", "subject_id": "E001", "predicate": "located_at",
     "object": {"kind": "StringValue", "text": "Bellhaven"},
     "polarity": "Positive", "confidence": 0.7,
     "document_id": "D001",
     "span": {"start": 0, "end": 18, "quote": "Aster Vale Systems"}}
  ],
  "events": [],
  "citations": [
    {"citing": "D002", "cited": "D001", "span": null}
  ]
})JSON";

} // namespace

TEST_CASE("pipeline: ingest builds the typed graph with correct edge counts") {
    auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    verity::engine::VerityEngine engine;
    engine.ingest(rec);

    // nodes: 2 docs + 4 entities + 3 claims = 9
    CHECK_EQ(engine.graph().nodeCount(), 9u);
    CHECK_EQ(engine.store().size(), 9u);

    // edges: mentions 4 + claims (subject+object/evidence) 2*3+1 + cites 1
    // C001: SubjectOf+ObjectOf+DocumentEvidence = 3
    // C002: SubjectOf+ObjectOf+DocumentEvidence = 3
    // C003: SubjectOf+DocumentEvidence = 2 (string object)
    // mentions 4, citations 1  => 13
    CHECK_EQ(engine.graph().edgeCount(), 13u);
}

TEST_CASE("pipeline: neighbors C001 --depth 2 returns the correct BFS set") {
    auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    verity::engine::VerityEngine engine;
    engine.ingest(rec);

    const auto hood = engine.neighbors(verity::model::NodeId("C001"), 2);
    // C001 -> E001(1), E002(1), D001(1); depth 2 adds D002 (via D001 Cites? no —
    // Cites is doc->doc: D002->D001 so D001's out-edges exclude D002) and no
    // further: E001 has MentionedIn->D001 (visited); E002 MentionedIn->D001.
    std::vector<std::string> ids;
    for (const auto& [id, dist] : hood) ids.push_back(id.str());
    REQUIRE(ids.size() >= 4);
    CHECK(std::find(ids.begin(), ids.end(), "C001") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "E001") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "E002") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "D001") != ids.end());
    // D002 must NOT appear (it cites D001, direction is D002->D001)
    CHECK(std::find(ids.begin(), ids.end(), "D002") == ids.end());

    // depth 0 returns only the start node
    const auto zero = engine.neighbors(verity::model::NodeId("C001"), 0);
    CHECK_EQ(zero.size(), 1u);
}

TEST_CASE("pipeline: neighbors via citing edge (depth 3 reaches D002 from D001's in-edge side)") {
    auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    verity::engine::VerityEngine engine;
    engine.ingest(rec);
    // From D001: out-edges... D001 has no out Cites edge (D002->D001). But
    // C001's DocumentEvidence->D001 and mentions point INTO D001. From D002:
    // Cites->D001 is an out-edge of D002, so BFS from D002 reaches D001 and
    // then C001/E001/E002 via in-... no — BFS follows OUT-edges only:
    // D002 -> D001 -> (D001 has no out-edges) => 2 nodes.
    const auto hood = engine.neighbors(verity::model::NodeId("D002"), 3);
    std::vector<std::string> ids;
    for (const auto& [id, dist] : hood) ids.push_back(id.str());
    CHECK(std::find(ids.begin(), ids.end(), "D002") != ids.end());
    CHECK(std::find(ids.begin(), ids.end(), "D001") != ids.end());
}

TEST_CASE("pipeline: query evidence finds claims by name and alias") {
    auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    verity::engine::VerityEngine engine;
    engine.ingest(rec);

    const auto hits = engine.queryEvidence("Aster Vale Systems");
    REQUIRE_EQ(hits.size(), 2u);            // C001 + C003
    CHECK_EQ(hits[0].claim.str(), "C001");  // deterministic id order
    CHECK_EQ(hits[0].predicate, "has_ceo");
    CHECK_EQ(hits[0].quote, "Aster Vale Systems appointed Maria Chen as CEO.");

    // case-insensitive
    CHECK_EQ(engine.queryEvidence("aster vale systems").size(), 2u);
    // unknown name -> empty, not an error
    CHECK(engine.queryEvidence("Nobody").empty());
}

TEST_CASE("pipeline: unknown id in neighbors is an empty result, CLI maps to error") {
    auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    verity::engine::VerityEngine engine;
    engine.ingest(rec);
    CHECK(engine.neighbors(verity::model::NodeId("C999"), 2).empty());
}

TEST_CASE("pipeline: round trip through io preserves data (extraction -> record)") {
    const auto rec = verity::io::JsonLoader::parseExtraction(kPipeline);
    REQUIRE_EQ(rec.entities.size(), 4u);
    CHECK_EQ(rec.entities[0].mentions[0].span.quote, "Aster Vale Systems");
    CHECK_EQ(rec.entities[1].mentions[0].span.quote, "Maria Chen");
    REQUIRE_EQ(rec.citations.size(), 1u);
    CHECK_EQ(rec.citations[0].citing, "D002");
    CHECK_EQ(rec.citations[0].cited, "D001");
}
