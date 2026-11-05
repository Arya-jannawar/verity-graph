// VERITY GRAPH — tests/io/test_jsonloader.cpp
// JsonLoader: structural + semantic validation. Positive path loads; every
// malformed input produces a ParseError with a precise message.

#include "doctest/doctest.h"

#include <string>

#include "verity/io/JsonLoader.hpp"
#include "verity/model/Predicates.hpp"
#include "verity/util/VerityError.hpp"

using verity::io::JsonLoader;

namespace {

const char* kGood = R"JSON({
  "schema_version": "1.0.0",
  "documents": [
    {"id": "D001", "filename": "d001.txt", "format": "txt",
     "normalized_text": "Aster Vale Systems is headquartered in Bellhaven.\n",
     "metadata": {}, "citations": []}
  ],
  "entities": [
    {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
     "aliases": [],
     "mentions": [{"document_id": "D001", "span": {"start": 0, "end": 18, "quote": "Aster Vale Systems"}}]}
  ],
  "claims": [
    {"id": "C001", "subject_id": "E001", "predicate": "located_at",
     "object": {"kind": "StringValue", "text": "Bellhaven"},
     "polarity": "Positive", "confidence": 0.9,
     "document_id": "D001",
     "span": {"start": 39, "end": 48, "quote": "Bellhaven"},
     "valid_from": null, "valid_to": null}
  ],
  "events": [],
  "citations": []
})JSON";

nlohmann::json goodJson() { return nlohmann::json::parse(kGood); }

} // namespace

TEST_CASE("JsonLoader: valid extraction loads fully") {
    const auto rec = JsonLoader::parseExtraction(kGood);
    CHECK_EQ(rec.schema_version, "1.0.0");
    REQUIRE_EQ(rec.documents.size(), 1u);
    CHECK_EQ(rec.documents[0].normalized_text, "Aster Vale Systems is headquartered in Bellhaven.\n");
    REQUIRE_EQ(rec.entities.size(), 1u);
    REQUIRE_EQ(rec.entities[0].mentions.size(), 1u);
    REQUIRE_EQ(rec.claims.size(), 1u);
    CHECK_EQ(rec.claims[0].predicate, "located_at");
    CHECK_EQ(rec.claims[0].span.quote, "Bellhaven");
    CHECK_FALSE(rec.claims[0].valid_from.has_value());
}

TEST_CASE("JsonLoader: span quote must match normalized text exactly (code points)") {
    auto j = goodJson();
    // wrong quote content
    j.at("claims").at(0).at("span").at("quote") = "Bellhavenn";
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    // wrong offsets (slice at those positions is different text)
    j = goodJson();
    j.at("claims").at(0).at("span").at("start") = 0;
    j.at("claims").at(0).at("span").at("end") = 8;
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    // out-of-range offsets
    j = goodJson();
    j.at("claims").at(0).at("span").at("end") = 10000;
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    // end < start
    j = goodJson();
    j.at("claims").at(0).at("span").at("end") = 3;
    j.at("claims").at(0).at("span").at("start") = 50;
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
}

TEST_CASE("JsonLoader: unicode spans count code points, not bytes") {
    nlohmann::json j = R"JSON({
      "schema_version": "1.0.0",
      "documents": [
        {"id": "D001", "filename": "d.txt", "format": "txt",
         "normalized_text": "Meet at Café Meridian.\n", "citations": []}
      ],
      "entities": [
        {"id": "E001", "name": "Café Meridian", "type": "Location",
         "mentions": [{"document_id": "D001",
                       "span": {"start": 8, "end": 21, "quote": "Café Meridian"}}]}
      ],
      "claims": [], "events": [], "citations": []
    })JSON"_json;
    // "Meet at Café Meridian." — code points: 'Café' é is ONE code point.
    // "Café Meridian" starts at cp 8, length 13 → [8, 21). Byte start = 9.
    const auto rec = JsonLoader::parseExtraction(j.dump());
    REQUIRE_EQ(rec.entities.size(), 1u);
    CHECK_EQ(rec.entities[0].mentions[0].span.start, 8u);
    CHECK_EQ(rec.entities[0].mentions[0].span.end, 21u);

    // byte-indexed span would be [9, 22] in code points — must FAIL here:
    j.at("entities").at(0).at("mentions").at(0).at("span").at("start") = 9;
    j.at("entities").at(0).at("mentions").at(0).at("span").at("end") = 22;
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
}

TEST_CASE("JsonLoader: structural failures") {
    SUBCASE("broken JSON") {
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction("{ not json "), verity::util::ParseError);
    }
    SUBCASE("missing schema_version") {
        auto j = goodJson();
        j.erase("schema_version");
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("bad schema_version") {
        auto j = goodJson();
        j.at("schema_version") = "9.9.9";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("unknown predicate") {
        auto j = goodJson();
        j.at("claims").at(0).at("predicate") = "has_boss";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("confidence outside [0,1]") {
        auto j = goodJson();
        j.at("claims").at(0).at("confidence") = 1.5;
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
        j.at("claims").at(0).at("confidence") = -0.5;
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("duplicate entity id") {
        auto j = goodJson();
        j.at("entities").push_back(j.at("entities").at(0));
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("unknown subject reference") {
        auto j = goodJson();
        j.at("claims").at(0).at("subject_id") = "E999";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("unknown EntityRef object reference") {
        auto j = goodJson();
        j.at("claims").at(0).at("object") = {{"kind", "EntityRef"}, {"entity_id", "E404"}};
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("unknown document reference in mention") {
        auto j = goodJson();
        j.at("entities").at(0).at("mentions").at(0).at("document_id") = "D999";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("malformed id") {
        auto j = goodJson();
        j.at("entities").at(0).at("id") = "ENTITY-1";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("normalized text without trailing newline") {
        auto j = goodJson();
        j.at("documents").at(0).at("normalized_text") = "no newline";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("normalized text with CR") {
        auto j = goodJson();
        j.at("documents").at(0).at("normalized_text") = "line one\r\nline two\n";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("unknown polarity") {
        auto j = goodJson();
        j.at("claims").at(0).at("polarity") = "Maybe";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("citation pointing at unknown document") {
        auto j = goodJson();
        j.at("documents").at(0).at("citations") = nlohmann::json::array({std::string("D777")});
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
    SUBCASE("bad date in valid_from") {
        auto j = goodJson();
        j.at("claims").at(0).at("valid_from") = "2024-99";
        CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
    }
}

TEST_CASE("JsonLoader: quantity objects require units") {
    auto j = goodJson();
    j.at("claims").at(0).at("object") = {{"kind", "QuantityValue"}, {"quantity", 5.0}, {"unit", ""}};
    CHECK_THROWS_AS((void)JsonLoader::parseExtraction(j.dump()), verity::util::ParseError);
}

TEST_CASE("predicate vocabulary is the frozen starter set") {
    CHECK(verity::model::isKnownPredicate("announced"));
    CHECK(verity::model::isKnownPredicate("launched"));
    CHECK(verity::model::isKnownPredicate("has_ceo"));
    CHECK(verity::model::isKnownPredicate("located_at"));
    CHECK(verity::model::isKnownPredicate("present_at"));
    CHECK(verity::model::isKnownPredicate("happened_before"));
    CHECK(verity::model::isKnownPredicate("cites"));
    CHECK_FALSE(verity::model::isKnownPredicate("announced_at"));
    CHECK_EQ(verity::model::knownPredicates().size(), 7u);
}
