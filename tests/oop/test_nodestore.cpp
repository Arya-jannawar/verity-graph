// VERITY GRAPH — tests/oop/test_nodestore.cpp
// NodeStore Rule of Five: deep copy via clone(), self-assignment safety,
// move-from validity, construction failure without leaks, ownership balance.

#include "doctest/doctest.h"

#include <memory>
#include <string>
#include <utility>

#include "verity/model/Entity.hpp"
#include "verity/model/Node.hpp"
#include "verity/model/NodeStore.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::model;

TEST_CASE("NodeStore: add/find/ownership") {
    NodeStore store("test");
    auto a = std::make_unique<Entity>("John Smith", EntityType::Person, std::vector<std::string>{}, "E700");
    Node* raw = a.get();
    Node* stored = store.add(std::move(a));
    CHECK(stored == raw);                       // returns the same object
    CHECK(store.find(NodeId("E700")) == raw);
    CHECK_EQ(store.size(), 1u);
    CHECK(store.find(NodeId("E999")) == nullptr);

    CHECK_THROWS_AS(store.add(nullptr), verity::util::VerityError);
    auto dup = std::make_unique<Entity>("Other", EntityType::Person, std::vector<std::string>{}, "E700");
    CHECK_THROWS_AS(store.add(std::move(dup)), verity::util::DuplicateNodeError);
    CHECK_EQ(store.size(), 1u);                 // failed add did not corrupt
    CHECK_EQ(store.findAs<Entity>(NodeId("E700"))->label(), "John Smith");
}

TEST_CASE("NodeStore: copy constructor deep-copies (mutating copy leaves original intact)") {
    NodeStore original("orig");
    original.add(std::make_unique<Entity>("John Smith", EntityType::Person,
                                          std::vector<std::string>{"J. Smith"}, "E701"));
    original.add(std::make_unique<Entity>("Aster Vale Systems",
                                          EntityType::Organization,
                                          std::vector<std::string>{}, "E702"));

    NodeStore copy(original);                    // deep copy
    CHECK_EQ(copy.size(), original.size());
    CHECK(copy.find(NodeId("E701")) != original.find(NodeId("E701")));  // distinct objects

    auto* copiedEntity = copy.findAs<Entity>(NodeId("E701"));
    REQUIRE(copiedEntity != nullptr);
    copiedEntity->setLabel("Mutated");
    copiedEntity->addAlias("NEW");
    CHECK_EQ(original.findAs<Entity>(NodeId("E701"))->label(), "John Smith");  // untouched
    CHECK_EQ(original.findAs<Entity>(NodeId("E701"))->aliases().size(), 1u);
    CHECK_EQ(copy.findAs<Entity>(NodeId("E701"))->aliases().size(), 2u);
}

TEST_CASE("NodeStore: copy assignment is self-assignment safe") {
    NodeStore store("self");
    store.add(std::make_unique<Entity>("Solo", EntityType::Person, std::vector<std::string>{}, "E703"));
    Node* before = store.find(NodeId("E703"));

    NodeStore& alias = store;
    store = alias;                               // self copy-assignment
    CHECK_EQ(store.size(), 1u);
    CHECK(store.find(NodeId("E703")) == before);
    CHECK_EQ(store.findAs<Entity>(NodeId("E703"))->label(), "Solo");
}

TEST_CASE("NodeStore: copy assignment strong-exception + value semantics") {
    NodeStore a("a");
    a.add(std::make_unique<Entity>("FromA", EntityType::Person, std::vector<std::string>{}, "E704"));
    NodeStore b("b");
    b.add(std::make_unique<Entity>("FromB", EntityType::Person, std::vector<std::string>{}, "E705"));
    a = b;
    CHECK_EQ(a.size(), 1u);
    CHECK_EQ(a.findAs<Entity>(NodeId("E705"))->label(), "FromB");
    CHECK(a.find(NodeId("E704")) == nullptr);    // old contents replaced
}

TEST_CASE("NodeStore: move constructor/assignment leave moved-from empty but valid") {
    const int before = Node::liveObjects();
    NodeStore src("src");
    src.add(std::make_unique<Entity>("Mover", EntityType::Person, std::vector<std::string>{}, "E706"));
    Node* raw = src.find(NodeId("E706"));

    NodeStore dst(std::move(src));               // move ctor
    CHECK(dst.find(NodeId("E706")) == raw);      // ownership transferred, no copy
    CHECK(src.size() == 0u);                     // moved-from: empty
    CHECK(src.find(NodeId("E706")) == nullptr);
    CHECK(src.name() == "moved-from");

    NodeStore target("target");
    target.add(std::make_unique<Entity>("Old", EntityType::Person, std::vector<std::string>{}, "E707"));
    target = std::move(dst);                     // move assignment frees old content
    CHECK(target.find(NodeId("E706")) == raw);
    CHECK(dst.size() == 0u);
    CHECK_EQ(Node::liveObjects(), before + 1);   // exactly one live node
}

TEST_CASE("NodeStore: destructor deletes every owned node (live-object balance)") {
    const int before = Node::liveObjects();
    {
        NodeStore store("burn");
        for (int i = 0; i < 25; ++i) {
            store.add(std::make_unique<Entity>("Person" + std::to_string(i),
                                               EntityType::Person));
        }
        CHECK_EQ(Node::liveObjects(), before + 25);
    }
    CHECK_EQ(Node::liveObjects(), before);
}

TEST_CASE("NodeStore: copies also balance the live-object counter") {
    const int before = Node::liveObjects();
    {
        NodeStore src("src");
        src.add(std::make_unique<Entity>("X", EntityType::Person, std::vector<std::string>{}, "E708"));
        NodeStore copy = src;
        CHECK_EQ(Node::liveObjects(), before + 2);
    }
    CHECK_EQ(Node::liveObjects(), before);
}

TEST_CASE("construction failure leaves no leak (exception from constructor)") {
    const int before = Node::liveObjects();
    try {
        // NodeId validation throws DURING construction; the object never exists
        Entity doomed("Broken", EntityType::Person, std::vector<std::string>{},
                      std::optional<std::string>("BAD-ID"));
        FAIL("construction should have thrown");
        (void)doomed;
    } catch (const verity::util::ParseError&) {
        // expected
    }
    CHECK_EQ(Node::liveObjects(), before);
}
