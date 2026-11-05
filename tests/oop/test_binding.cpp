// VERITY GRAPH — tests/oop/test_binding.cpp
// Static vs dynamic binding demonstration.
//
//   * `type()` / `describe()` are VIRTUAL: calls through Node& resolve at
//     runtime to the most-derived override (DYNAMIC binding).
//   * `id()` is NON-virtual: calls through Node& resolve at compile time to
//     Node::id (STATIC binding) — correct, because non-virtual members are
//     not overridden.
//   * A non-virtual shadowed function demonstrates the classic static-binding
//     pitfall: through base pointers the BASE version runs.

#include "doctest/doctest.h"

#include <string>

#include "verity/model/ClaimNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/EventNode.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::model;

namespace {

/// Shadowing demo: Base::tag is non-virtual; Derived shadows it.
class BindBase {
public:
    virtual ~BindBase() = default;
    [[nodiscard]] std::string tag() const { return "base"; }         // static
    [[nodiscard]] virtual std::string vtag() const { return "vbase"; }  // dynamic
};

class BindDerived final : public BindBase {
public:
    [[nodiscard]] std::string tag() const { return "derived"; }      // shadows
    [[nodiscard]] std::string vtag() const override { return "vderived"; }
};

} // namespace

TEST_CASE("dynamic binding: virtual calls through base references dispatch at runtime") {
    Entity entity("John Smith", EntityType::Person);
    ClaimNode claim("c", NodeId("E001"), "has_ceo", std::string("X"), NodeId("D001"),
                    Span{0, 3, "abc"});
    EventNode event("launch", NodeId("E001"), DateInterval::fromString("2024-03"),
                    NodeId("D001"), Span{0, 3, "abc"});

    const Node* nodes[] = {&entity, &claim, &event};
    CHECK(std::string(nodes[0]->type()) == "Entity");
    CHECK(std::string(nodes[1]->type()) == "Claim");
    CHECK(std::string(nodes[2]->type()) == "Event");

    // describe() is virtual: each object prints its own dynamic shape
    CHECK(std::string(nodes[0]->describe()).find("Entity[Person]") != std::string::npos);
    CHECK(std::string(nodes[1]->describe()).find("Claim:") != std::string::npos);
    CHECK(std::string(nodes[2]->describe()).find("Event") != std::string::npos);
}

TEST_CASE("static binding: non-virtual calls through base references use the base version") {
    BindBase* basePtr = new BindDerived();
    CHECK_EQ(basePtr->tag(), std::string("base"));       // static binding pitfall
    CHECK_EQ(basePtr->vtag(), std::string("vderived"));  // dynamic binding
    delete basePtr;

    BindDerived derived;
    BindBase& baseRef = derived;
    CHECK_EQ(baseRef.tag(), std::string("base"));
    CHECK_EQ(derived.tag(), std::string("derived"));     // direct: derived version
}

TEST_CASE("model classes: non-virtual accessors are statically bound (and correct)") {
    // Node::id is non-virtual and never shadowed; static binding is safe here.
    Entity entity("Aster Vale Systems", EntityType::Organization, {}, "E800");
    const Node& ref = entity;
    CHECK_EQ(ref.id().str(), std::string("E800"));
    CHECK_EQ(entity.id().str(), std::string("E800"));
}
