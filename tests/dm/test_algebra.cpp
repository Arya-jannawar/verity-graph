// VERITY GRAPH — tests/dm/test_algebra.cpp
// Algebra law checkers: semigroup/monoid/group over (Z_n,+), (Z_p*,x),
// subgroup closure, cyclic generators, Lagrange, homomorphism (with an honest
// failing case), and the AliasSet monoid (Entity alias-union domain).

#include "doctest/doctest.h"

#include <cstdint>
#include <numeric>
#include <vector>

#include "verity/dm/Algebra.hpp"
#include "verity/model/Entity.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::findIdentity;
using verity::dm::groupHolds;
using verity::dm::homomorphismHolds;
using verity::dm::lagrangeHolds;
using verity::dm::monoidHolds;
using verity::dm::semigroupHolds;
using verity::dm::subgroupHolds;
using verity::dm::ZnPlus;
using verity::dm::ZpMul;

TEST_CASE("(Z_n, +) is a group for several n; generators are the coprimes") {
    for (const std::uint64_t n : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 12u}) {
        const ZnPlus zn{n};
        CHECK(groupHolds(zn.elements(), [&zn](std::uint64_t a, std::uint64_t b) { return zn.op(a, b); },
                         zn.identity()));
        // exhaustive generator check: <k> == whole group iff gcd(k,n) == 1
        for (std::uint64_t k = 0; k < n; ++k) {
            const auto generated = zn.subgroupGeneratedBy(k);
            const bool isWhole = generated.size() == n;
            CHECK_EQ(isWhole, std::gcd(k, n) == 1);
        }
    }
    // Z_6 generators: exactly 1 and 5
    const ZnPlus z6{6};
    const auto gens = z6.cyclicGenerators();
    REQUIRE_EQ(gens.size(), 2u);
    CHECK_EQ(gens[0], 1u);
    CHECK_EQ(gens[1], 5u);
}

TEST_CASE("(Z_p*, x) is a group for small primes; honest failure for composites") {
    for (const std::uint64_t p : {3u, 5u, 7u, 11u, 13u}) {
        const ZpMul zp{p};
        CHECK(groupHolds(zp.elements(), [&zp](std::uint64_t a, std::uint64_t b) { return zp.op(a, b); },
                         zp.identity()));
        // Z_p* is cyclic of order p-1: number of generators = phi(p-1)
        CHECK_EQ(zp.cyclicGenerators().size(),
                 static_cast<std::size_t>(verity::dm::eulerTotient(p - 1)));
    }
    CHECK_THROWS_AS(ZpMul(6), verity::util::ParseError);   // 6 is not prime

    // honest failing case: (Z_6, x) with identity 1 is NOT a group
    // (2, 3, 4 have no inverses; 0 has none either)
    const std::vector<int> z6{0, 1, 2, 3, 4, 5};
    const auto mul6 = [](int a, int b) { return (a * b) % 6; };
    CHECK_FALSE(groupHolds(z6, mul6, 1));
}

TEST_CASE("subgroup closure: <2> in Z_6 and honest failure of {0,2,4} in Z_5") {
    const ZnPlus z6{6};
    const auto sub = z6.subgroupGeneratedBy(2);   // {0, 2, 4}
    CHECK(subgroupHolds(z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
                        z6.identity(), sub));
    CHECK_EQ(sub.size(), 3u);
    CHECK(lagrangeHolds(6, sub.size()));

    // {0,2,4} is NOT a subgroup of Z_5 (elements not even members)
    const ZnPlus z5{5};
    CHECK_FALSE(subgroupHolds(z5.elements(),
                              [&z5](std::uint64_t a, std::uint64_t b) { return z5.op(a, b); },
                              z5.identity(), std::vector<std::uint64_t>{0, 2, 4}));
    // non-closed subset {0,1,3} in Z_6: 1+3=4 not in subset
    CHECK_FALSE(subgroupHolds(z6.elements(),
                              [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
                              z6.identity(), std::vector<std::uint64_t>{0, 1, 3}));
}

TEST_CASE("Lagrange: subgroup orders divide the group order") {
    const ZnPlus z12{12};
    for (std::uint64_t k = 1; k < 12; ++k) {
        const auto sub = z12.subgroupGeneratedBy(k);
        CHECK(lagrangeHolds(12, sub.size()));
        // exhaustive check: the order really is 12 / gcd(12, k)
        CHECK_EQ(sub.size(), 12 / std::gcd(12u, k));
    }
    CHECK_FALSE(lagrangeHolds(7, 3));
}

TEST_CASE("homomorphism: honest passing and failing examples") {
    // f(x) = x + 1 (mod 6) is NOT a homomorphism (Z_6,+ -> Z_6,+):
    // f(3) = 4 but f(1) + f(2) = 2 + 3 = 5 -> mismatch. The checker must
    // return FALSE. (Note f(x) = 2x IS a homomorphism — linear maps always
    // are — so it is NOT used as the failure example.)
    const ZnPlus z6{6};
    const auto failing = homomorphismHolds(
        z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
        z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
        [](std::uint64_t x) { return (x + 1) % 6; });
    CHECK_FALSE(failing);

    // f(x) = x (identity) IS a homomorphism
    const auto identity = homomorphismHolds(
        z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
        z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
        [](std::uint64_t x) { return x; });
    CHECK(identity);

    // f(x) = 2x mod 6 IS a homomorphism Z_6 -> Z_3 (mod-3 reduction of it):
    // phi(x) = (2x) mod 3 with operations + mod 6 / + mod 3
    const ZnPlus z3{3};
    const auto quotient = homomorphismHolds(
        z6.elements(), [&z6](std::uint64_t a, std::uint64_t b) { return z6.op(a, b); },
        z3.elements(), [&z3](std::uint64_t a, std::uint64_t b) { return z3.op(a, b); },
        [](std::uint64_t x) { return (2 * x) % 3; });
    CHECK(quotient);
}

TEST_CASE("AliasSet: (aliases, +, empty) is a commutative monoid") {
    using verity::model::AliasSet;
    const AliasSet empty = AliasSet::identity();
    const AliasSet a{{"J. Smith", "John Smith"}};
    const AliasSet b{{"Mr. Smith"}};

    // closure: a + b is an AliasSet (type check by construction)
    const AliasSet ab = a + b;
    CHECK_EQ(ab.aliases.size(), 3u);

    // associativity over the three-element domain, exhaustively
    const std::vector<AliasSet> domain{empty, a, b, ab};
    for (const AliasSet& x : domain) {
        for (const AliasSet& y : domain) {
            for (const AliasSet& z : domain) {
                CHECK_EQ((x + y) + z, x + (y + z));
            }
        }
    }
    // identity both sides
    for (const AliasSet& x : domain) {
        CHECK_EQ(empty + x, x);
        CHECK_EQ(x + empty, x);
    }
    // commutativity
    CHECK_EQ(a + b, b + a);
}

TEST_CASE("Entity operators: ==, <, alias-union +") {
    using verity::model::Entity;
    using verity::model::EntityType;
    // == is strict identity: same name, same type, same alias set
    Entity johnA("John Smith", EntityType::Person, {"J. Smith"}, "E005");
    Entity johnB("John Smith", EntityType::Person, {"J. Smith"}, "E010");
    CHECK(johnA == johnB);
    CHECK_FALSE(johnA != johnB);

    Entity jsmith("J. Smith", EntityType::Person, {"John Smith"}, "E011");
    CHECK(johnA != jsmith);   // different names: not identical

    Entity other("Adam Smith", EntityType::Person, {}, "E020");
    CHECK(johnA != other);
    CHECK((johnA < other) != (other < johnA));   // antisymmetric total order

    // + merges aliases of SAME-named entities
    Entity johnC("John Smith", EntityType::Person, {"Mr. Smith", "Smith, John"}, "E012");
    Entity merged = johnA + johnC;
    CHECK_EQ(merged.aliases().size(), 3u);   // J. Smith, Mr. Smith, Smith, John

    // + requires matching names (honest failure)
    CHECK_THROWS_AS((void)(johnA + jsmith), verity::util::VerityError);
}
