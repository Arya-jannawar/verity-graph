// VERITY GRAPH — tests/oop/test_model.cpp
// OOP Units 1-2: encapsulation (validated setters), constructor overloading +
// default arguments, static ID generation + live-object counter, this-pointer
// chaining, friend operator<< and GraphInspector, const correctness.

#include "doctest/doctest.h"

#include <sstream>
#include <string>

#include "verity/model/ClaimNode.hpp"
#include "verity/model/DocumentNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/EventNode.hpp"
#include "verity/model/GraphInspector.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::model;

TEST_CASE("NodeId: validation and canonicalization") {
    CHECK_EQ(NodeId("D6").str(), std::string("D006"));
    CHECK_EQ(NodeId("D006").str(), std::string("D006"));
    CHECK_EQ(NodeId("EV12").str(), std::string("EV012"));
    CHECK_EQ(NodeId("E001").str(), std::string("E001"));
    CHECK_EQ(NodeId("C42").str(), std::string("C042"));
    CHECK_THROWS_AS(NodeId("X001"), verity::util::ParseError);
    CHECK_THROWS_AS(NodeId("D"), verity::util::ParseError);
    CHECK_THROWS_AS(NodeId("Dabc"), verity::util::ParseError);
    CHECK_THROWS_AS(NodeId("D000"), verity::util::ParseError);
    CHECK_THROWS_AS(NodeId(""), verity::util::ParseError);
    CHECK(NodeId("E001") == NodeId("E1"));
    CHECK(NodeId("E001") < NodeId("E002"));
}

TEST_CASE("encapsulation: setConfidence validates [0,1]") {
    ClaimNode claim("has_ceo", NodeId("E001"), "has_ceo", std::string("Maria Chen"),
                    NodeId("D001"), Span{0, 5, "hello"});
    CHECK_EQ(claim.confidence(), doctest::Approx(0.8));   // default argument
    claim.setConfidence(0.0);
    claim.setConfidence(1.0);
    CHECK_THROWS_AS(claim.setConfidence(-0.1), verity::util::VerityError);
    CHECK_THROWS_AS(claim.setConfidence(1.01), verity::util::VerityError);
    CHECK_THROWS_AS(claim.withConfidence(2.0), verity::util::VerityError);
    CHECK_EQ(claim.confidence(), doctest::Approx(1.0));   // unchanged after throws
}

TEST_CASE("constructor overloading: string-object convenience constructor") {
    ClaimNode a("p", NodeId("E001"), "has_ceo", std::string("Maria Chen"),
                NodeId("D001"), Span{0, 3, "abc"}, Polarity::Positive, 0.9);
    CHECK_EQ(a.object().kind, ClaimObject::Kind::StringValue);
    CHECK_EQ(a.object().text, "Maria Chen");
    CHECK_EQ(a.polarity(), Polarity::Positive);
    CHECK_EQ(a.confidence(), doctest::Approx(0.9));
}

TEST_CASE("static nextId: per-prefix counters are stable and padded") {
    const std::string a = Node::nextId("C");
    const std::string b = Node::nextId("C");
    CHECK_NE(a, b);
    CHECK_EQ(a.substr(0, 1), std::string("C"));
    CHECK_EQ(b.substr(0, 1), std::string("C"));
    CHECK_EQ(a.size(), 4u);   // C### padded
    CHECK_THROWS_AS((void)Node::nextId("X"), verity::util::ParseError);
}

TEST_CASE("observeId: counters resume above loaded ids (no post-load collisions)") {
    Node::observeId("C777");
    const std::string next = Node::nextId("C");
    CHECK_EQ(next, std::string("C778"));
    Node::observeId("EV005");
    CHECK_EQ(Node::nextId("EV"), std::string("EV006"));
}

TEST_CASE("live-object counter balances across construction and destruction") {
    const int before = Node::liveObjects();
    {
        Entity e("Temp Org", EntityType::Organization);
        CHECK_EQ(Node::liveObjects(), before + 1);
        {
            auto clone = e.clone();
            CHECK_EQ(Node::liveObjects(), before + 2);
        }
        CHECK_EQ(Node::liveObjects(), before + 1);
    }
    CHECK_EQ(Node::liveObjects(), before);
}

TEST_CASE("this-pointer chaining on ClaimNode") {
    ClaimNode claim("p", NodeId("E001"), "located_at", std::string("Bellhaven"),
                    NodeId("D001"), Span{0, 3, "abc"});
    claim.withConfidence(0.95).withPolarity(Polarity::Negative).withValidTime(
        DateInterval::fromString("2024-03"));
    CHECK_EQ(claim.confidence(), doctest::Approx(0.95));
    CHECK_EQ(claim.polarity(), Polarity::Negative);
    REQUIRE(claim.validTime().has_value());
    CHECK_EQ(claim.validTime()->toString(), std::string("2024-03"));
}

TEST_CASE("friend operator<< dispatches virtually through describe()") {
    Entity e("John Smith", EntityType::Person, {"J. Smith"}, "E999");
    std::ostringstream os;
    os << e;                              // operator<<(ostream&, const Node&)
    const std::string text = os.str();
    CHECK(text.find("E999") != std::string::npos);
    CHECK(text.find("Entity[Person]") != std::string::npos);
    CHECK(text.find("John Smith") != std::string::npos);
    CHECK(text.find("J. Smith") != std::string::npos);

    const Entity& baseRef = e;            // const-correct access
    CHECK(baseRef.type() == "Entity");
    std::ostringstream os2;
    os2 << baseRef;
    CHECK_EQ(os2.str(), text);
}

TEST_CASE("friend GraphInspector reads private state") {
    ClaimNode claim("p", NodeId("E001"), "has_ceo", std::string("X"), NodeId("D001"),
                    Span{0, 3, "abc"}, Polarity::Positive, 0.42, "C900");
    CHECK_EQ(GraphInspector::confidenceOf(claim), doctest::Approx(0.42));

    Entity e("Elena Vasquez", EntityType::Person, {"Lena V."}, "E901");
    CHECK_EQ(GraphInspector::firstAliasOf(e), std::string("Lena V."));

    const nlohmann::json dump = GraphInspector::inspect(e);
    CHECK_EQ(dump["dynamic_type"], "Entity");
    CHECK_EQ(dump["aliases"].at(0), "Lena V.");
    CHECK_EQ(dump["id"], "E901");
}

TEST_CASE("clone-based polymorphic copying preserves dynamic type") {
    Entity original("Aster Vale Systems", EntityType::Organization, {}, "E902");
    const std::unique_ptr<Node> copied = original.clone();
    CHECK(copied->type() == "Entity");
    const auto* asEntity = dynamic_cast<const Entity*>(copied.get());
    REQUIRE(asEntity != nullptr);
    CHECK(asEntity->entityType() == EntityType::Organization);
    CHECK(asEntity->id() == original.id());
}

TEST_CASE("EventNode date interval semantics") {
    // month precision overlaps a day inside the same month (NOT contradictory)
    const DateInterval month = DateInterval::fromString("2024-03");
    const DateInterval day = DateInterval::fromString("2024-03-15");
    CHECK(month.overlaps(day));
    CHECK_FALSE(month.strictlyBefore(day));
    // disjoint months
    const DateInterval june = DateInterval::fromString("2024-06");
    CHECK_FALSE(month.overlaps(june));
    CHECK(month.strictlyBefore(june));
    // year precision spans the whole year
    const DateInterval year = DateInterval::fromString("2024");
    CHECK(year.overlaps(june));
    CHECK(year.overlaps(day));
    // range interval
    const nlohmann::json range = {{"from", "2024-03-01"}, {"to", "2024-03-31"}};
    const DateInterval march = DateInterval::fromJson(range);
    CHECK(march.overlaps(day));
    CHECK(march.overlaps(month));
    CHECK_THROWS_AS((void)DateInterval::fromString("2024-13"), verity::util::ParseError);
    CHECK_THROWS_AS((void)DateInterval::fromString("2024-02-30"), verity::util::ParseError);
}

TEST_CASE("DocumentNode stores normalized text and citations") {
    DocumentNode doc("d001.txt", DocumentFormat::Txt, "hello world\n", {"D002"}, "D999");
    CHECK_EQ(doc.normalizedText(), "hello world\n");
    CHECK_EQ(doc.citations().size(), 1u);
    CHECK_EQ(doc.type(), std::string("Document"));
    CHECK_EQ(doc.format(), DocumentFormat::Txt);
}

TEST_CASE("labels are validated (encapsulation)") {
    Entity e("Valid", EntityType::Person);
    CHECK_THROWS_AS(e.setLabel(""), verity::util::VerityError);
    CHECK_THROWS_AS(e.setLabel("   "), verity::util::VerityError);
    CHECK_THROWS_WITH_AS(Entity("", EntityType::Person), "Node label must be non-empty",
                         verity::util::VerityError);
}
