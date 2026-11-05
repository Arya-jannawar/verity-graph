// VERITY GRAPH — tests/dm/test_functions.cpp
// FiniteFunction: totality, injectivity, surjectivity, bijectivity,
// composition, inverse, failure paths, and the mention→canonical totality
// check pattern.

#include "doctest/doctest.h"

#include "verity/dm/Functions.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::FiniteFunction;
using verity::dm::FiniteSet;

TEST_CASE("FiniteFunction: classification of known functions") {
    const auto domain = FiniteSet<int>::fromVector({1, 2, 3});
    const auto codomain = FiniteSet<int>::fromVector({4, 5, 6});

    const FiniteFunction<int, int> bijective(domain, codomain, {{1, 4}, {2, 5}, {3, 6}});
    CHECK(bijective.isTotal());
    CHECK(bijective.isInjective());
    CHECK(bijective.isSurjective());
    CHECK(bijective.isBijective());

    const FiniteFunction<int, int> constant(domain, codomain, {{1, 4}, {2, 4}, {3, 4}});
    CHECK(constant.isTotal());
    CHECK_FALSE(constant.isInjective());
    CHECK_FALSE(constant.isSurjective());   // 5,6 never hit

    // partial function (2 missing): not total
    const FiniteFunction<int, int> partial(domain, codomain, {{1, 4}, {3, 6}});
    CHECK_FALSE(partial.isTotal());
    CHECK(partial.isInjective());
}

TEST_CASE("FiniteFunction: surjective but not injective") {
    const auto domain = FiniteSet<int>::fromVector({1, 2, 3, 4});
    const auto codomain = FiniteSet<int>::fromVector({0, 1});
    const FiniteFunction<int, int> parity(domain, codomain,
                                         {{1, 1}, {2, 0}, {3, 1}, {4, 0}});
    CHECK(parity.isSurjective());
    CHECK_FALSE(parity.isInjective());
    CHECK(parity.isTotalSurjective());
    CHECK_FALSE(parity.isBijective());
}

TEST_CASE("FiniteFunction: composition and inverse") {
    const auto sq = FiniteSet<int>::fromVector({0, 1, 2});
    const FiniteFunction<int, int> cycle(sq, sq, {{0, 1}, {1, 2}, {2, 0}});
    const auto twice = FiniteFunction<int, int>::compose(cycle, cycle);
    CHECK(*twice.apply(0) == 2);
    CHECK(*twice.apply(1) == 0);
    CHECK(*twice.apply(2) == 1);
    const auto inv = cycle.inverse();
    CHECK(*inv.apply(1) == 0);
    CHECK(*inv.apply(2) == 1);
    CHECK(*inv.apply(0) == 2);

    // inverse of a non-bijection must throw
    const auto dom3 = FiniteSet<int>::fromVector({1, 2, 3});
    const auto cod2 = FiniteSet<int>::fromVector({0, 1});
    const FiniteFunction<int, int> notInjective(dom3, cod2, {{1, 0}, {2, 1}, {3, 1}});
    CHECK_THROWS_AS((void)notInjective.inverse(), verity::util::VerityError);

    // composition where the inner image escapes the outer domain must throw
    const auto dom2 = FiniteSet<int>::fromVector({1, 2});
    const auto cod3 = FiniteSet<int>::fromVector({4, 5, 6});
    const FiniteFunction<int, int> into3(dom2, cod3, {{1, 4}, {2, 5}});
    const FiniteFunction<int, int> on3(cod3, cod3, {{4, 5}, {5, 6}, {6, 4}});
    using FF = FiniteFunction<int, int>;
    const auto composeOk = [&] { return FF::compose(on3, into3); };
    CHECK_NOTHROW((void)composeOk());
    const FiniteFunction<int, int> on2(dom2, dom2, {{1, 2}, {2, 1}});
    const auto composeBad = [&] { return FF::compose(on2, into3); };
    CHECK_THROWS_AS((void)composeBad(), verity::util::VerityError);
}

TEST_CASE("FiniteFunction: construction failure paths") {
    const auto domain = FiniteSet<int>::fromVector({1, 2});
    const auto codomain = FiniteSet<int>::fromVector({3, 4});
    // mapping outside the domain
    CHECK_THROWS_AS((FiniteFunction<int, int>(domain, codomain, {{9, 3}})),
                    verity::util::VerityError);
    // mapping outside the codomain
    CHECK_THROWS_AS((FiniteFunction<int, int>(domain, codomain, {{1, 9}})),
                    verity::util::VerityError);
    // an element mapped twice
    CHECK_THROWS_AS((FiniteFunction<int, int>(domain, codomain, {{1, 3}, {1, 4}})),
                    verity::util::VerityError);
}

TEST_CASE("FiniteFunction: mention-to-canonical mapping is total and onto") {
    // the EntityResolver property, demonstrated at the dm level:
    // every mention maps to a canonical entity and every canonical entity is hit
    const auto mentions = FiniteSet<std::string>::fromVector(
        {"J. Smith", "Mr. Smith", "Smith, John", "John Smith"});
    const auto canonical = FiniteSet<std::string>::fromVector({"John Smith"});
    const FiniteFunction<std::string, std::string> resolver(
        mentions, canonical,
        {{"J. Smith", "John Smith"},
         {"Mr. Smith", "John Smith"},
         {"Smith, John", "John Smith"},
         {"John Smith", "John Smith"}});
    CHECK(resolver.isTotalSurjective());
    CHECK_FALSE(resolver.isInjective());   // 4 mentions -> 1 canonical
}
