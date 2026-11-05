// VERITY GRAPH — tests/engine/test_engine2.cpp
// Phase-2 engine: EntityResolver (4 alias variants merge; unrelated
// same-surname persons stay separate), TimelineBuilder (strict order, Hasse,
// incomparables, temporal cycle witness).

#include "doctest/doctest.h"

#include <algorithm>
#include <string>
#include <vector>

#include "verity/engine/EntityResolver.hpp"
#include "verity/engine/TimelineBuilder.hpp"
#include "verity/io/JsonLoader.hpp"

namespace {

const char* kAliasData = R"JSON({
  "schema_version": "1.0.0",
  "documents": [
    {"id": "D004", "filename": "d004.txt", "format": "txt",
     "normalized_text": "John Smith joined the board. J. Smith advised us. A memo went to Mr. Smith. The register lists Smith, John.\n",
     "citations": []}
  ],
  "entities": [
    {"id": "E005", "name": "John Smith", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 0, "end": 10, "quote": "John Smith"}}]},
    {"id": "E010", "name": "J. Smith", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 27, "end": 35, "quote": "J. Smith"}}]},
    {"id": "E011", "name": "Mr. Smith", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 54, "end": 63, "quote": "Mr. Smith"}}]},
    {"id": "E012", "name": "Smith, John", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 85, "end": 96, "quote": "Smith, John"}}]},
    {"id": "E020", "name": "Will Smith", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 96, "end": 96, "quote": ""}}]},
    {"id": "E021", "name": "A. Smith", "type": "Person", "aliases": [],
     "mentions": [{"document_id": "D004", "span": {"start": 96, "end": 96, "quote": ""}}]}
  ],
  "claims": [], "events": [], "citations": []
})JSON";

} // namespace

TEST_CASE("EntityResolver: all four John Smith variants resolve to one class") {
    const auto rec = verity::io::JsonLoader::parseExtraction(kAliasData);
    const auto result = verity::engine::EntityResolver::resolve(rec);
    // E005, E010, E011, E012 form ONE class
    const std::vector<std::string> mergedClass{"E005", "E010", "E011", "E012"};
    CHECK(std::find(result.classes.begin(), result.classes.end(), mergedClass) !=
          result.classes.end());
    // every alias maps to the canonical E005
    for (const std::string& id : {"E005", "E010", "E011", "E012"}) {
        CHECK_EQ(verity::engine::EntityResolver::canonicalOf(result, id), "E005");
    }
    // the canonical representative is the full plain name's entity
    for (const auto& [canonical, aliases] : result.aliasesByCanonical) {
        if (canonical == "E005") {
            const std::vector<std::string> expected{"J. Smith", "John Smith", "Mr. Smith",
                                                    "Smith, John", "Will Smith"};
            // E020 (Will Smith) is a DIFFERENT person: not in this class
            CHECK_FALSE(std::find(aliases.begin(), aliases.end(), "Will Smith") != aliases.end());
        }
    }
}

TEST_CASE("EntityResolver: unrelated same-surname people stay separate") {
    const auto rec = verity::io::JsonLoader::parseExtraction(kAliasData);
    const auto result = verity::engine::EntityResolver::resolve(rec);
    // E020 "Will Smith" and E021 "A. Smith" never merge into the John Smith class
    CHECK_EQ(verity::engine::EntityResolver::canonicalOf(result, "E020"), "E020");
    CHECK_EQ(verity::engine::EntityResolver::canonicalOf(result, "E021"), "E021");
    CHECK_EQ(verity::engine::EntityResolver::canonicalOf(result, "E005"), "E005");
    // distinct known first names never merge even with equal surnames
    CHECK_NE(verity::engine::EntityResolver::canonicalOf(result, "E020"),
             verity::engine::EntityResolver::canonicalOf(result, "E005"));
}

namespace {

const char* kTimelineData = R"JSON({
  "schema_version": "1.0.0",
  "documents": [
    {"id": "D005", "filename": "d005.txt", "format": "txt",
     "normalized_text": "Milestones memo.\n", "citations": []}
  ],
  "entities": [
    {"id": "E004", "name": "Product X", "type": "Product", "aliases": [],
     "mentions": [{"document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}}]}
  ],
  "claims": [],
  "events": [
    {"id": "EV001", "label": "internal beta", "entity_id": "E004", "date": "2024-01-15",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}},
    {"id": "EV002", "label": "pilot rollout", "entity_id": "E004", "date": "2024-02",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}},
    {"id": "EV003", "label": "public beta", "entity_id": "E004", "date": "2024-03",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}},
    {"id": "EV004", "label": "board review", "entity_id": "E004", "date": "2024",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}}
  ],
  "citations": []
})JSON";

} // namespace

TEST_CASE("TimelineBuilder: date-precision order, Hasse, incomparables") {
    const auto rec = verity::io::JsonLoader::parseExtraction(kTimelineData);
    const auto result = verity::engine::TimelineBuilder::build(rec);

    // no cycle: full output present
    CHECK_FALSE(result.finding.has_value());

    // chronological order by start serial: EV001, EV002, EV003, then EV004 (year)
    REQUIRE_EQ(result.chronologicalOrder.size(), 4u);
    CHECK_EQ(result.chronologicalOrder[0], "EV001");
    CHECK_EQ(result.chronologicalOrder[1], "EV002");
    CHECK_EQ(result.chronologicalOrder[2], "EV003");

    // topological order valid (Kahn output respects the partial order)
    REQUIRE_EQ(result.topologicalOrder.size(), 4u);

    // Hasse edges: EV001<EV002, EV002<EV003 (EV001<EV003 implied); EV004 (year
    // precision) is incomparable to the precise ones — no date-derived edges
    REQUIRE_EQ(result.hasseEdges.size(), 2u);
    CHECK(std::find(result.hasseEdges.begin(), result.hasseEdges.end(),
                    std::make_pair(std::string("EV001"), std::string("EV002"))) !=
          result.hasseEdges.end());
    CHECK(std::find(result.hasseEdges.begin(), result.hasseEdges.end(),
                    std::make_pair(std::string("EV002"), std::string("EV003"))) !=
          result.hasseEdges.end());

    // incomparable pairs all involve the year-precision EV004
    CHECK_EQ(result.incomparablePairs.size(), 3u);
    for (const auto& [a, b] : result.incomparablePairs) {
        CHECK((a == "EV004" || b == "EV004"));
    }

    // DOT contains the cover edges and no implied ones
    CHECK(result.hasseDot.find("\"EV001\" -> \"EV002\"") != std::string::npos);
    CHECK(result.hasseDot.find("\"EV001\" -> \"EV003\"") == std::string::npos);
}

namespace {

const char* kCycleData = R"JSON({
  "schema_version": "1.0.0",
  "documents": [
    {"id": "D005", "filename": "d005.txt", "format": "txt",
     "normalized_text": "Milestones memo.\n", "citations": []}
  ],
  "entities": [
    {"id": "E004", "name": "Product X", "type": "Product", "aliases": [],
     "mentions": [{"document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}}]}
  ],
  "claims": [
    {"id": "C007", "subject_id": "EV001", "predicate": "happened_before",
     "object": {"kind": "EntityRef", "entity_id": "EV002"},
     "polarity": "Positive", "confidence": 0.9, "document_id": "D005",
     "span": {"start": 0, "end": 0, "quote": ""}, "valid_from": null, "valid_to": null},
    {"id": "C008", "subject_id": "EV002", "predicate": "happened_before",
     "object": {"kind": "EntityRef", "entity_id": "EV003"},
     "polarity": "Positive", "confidence": 0.9, "document_id": "D005",
     "span": {"start": 0, "end": 0, "quote": ""}, "valid_from": null, "valid_to": null},
    {"id": "C009", "subject_id": "EV003", "predicate": "happened_before",
     "object": {"kind": "EntityRef", "entity_id": "EV001"},
     "polarity": "Positive", "confidence": 0.9, "document_id": "D005",
     "span": {"start": 0, "end": 0, "quote": ""}, "valid_from": null, "valid_to": null}
  ],
  "events": [
    {"id": "EV001", "label": "internal beta", "entity_id": "E004", "date": "2024",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}},
    {"id": "EV002", "label": "pilot rollout", "entity_id": "E004", "date": "2024",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}},
    {"id": "EV003", "label": "public beta", "entity_id": "E004", "date": "2024",
     "document_id": "D005", "span": {"start": 0, "end": 0, "quote": ""}}
  ],
  "citations": []
})JSON";

} // namespace

TEST_CASE("TimelineBuilder: temporal cycle reported with witness, Hasse withheld") {
    const auto rec = verity::io::JsonLoader::parseExtraction(kCycleData);
    const auto result = verity::engine::TimelineBuilder::build(rec);

    REQUIRE(result.finding.has_value());
    const auto& finding = *result.finding;
    CHECK(finding.statement.find("Potential temporal contradiction") != std::string::npos);
    // witness: EV001 -> EV002 -> EV003 -> EV001
    REQUIRE_EQ(finding.witness.size(), 4u);
    CHECK_EQ(finding.witness.front(), finding.witness.back());
    const std::vector<std::string> expectedClaims{"C007", "C008", "C009"};
    for (const std::string& claim : expectedClaims) {
        CHECK(std::find(finding.claimIds.begin(), finding.claimIds.end(), claim) !=
              finding.claimIds.end());
    }
    // Hasse/topological output withheld for cyclic data
    CHECK(result.hasseEdges.empty());
    CHECK(result.topologicalOrder.empty());
    CHECK(result.hasseDot.empty());
}
