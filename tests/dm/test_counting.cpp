// VERITY GRAPH — tests/dm/test_counting.cpp
// Counting: factorial known values + overflow, nCr/nPr known values +
// overflow, Pascal-vs-multiplicative agreement over a full grid, binomial
// theorem identity checks.

#include "doctest/doctest.h"

#include <cstdint>
#include <vector>

#include "verity/dm/Counting.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::binomialIdentityCheck;
using verity::dm::binomialTheoremCheck;
using verity::dm::factorial;
using verity::dm::nCr;
using verity::dm::nPr;
using verity::dm::pascalNcr;

TEST_CASE("factorial: exact 0..20 and overflow throw at 21") {
    static const std::uint64_t fact[21] = {
        1ULL,
        1ULL,
        2ULL,
        6ULL,
        24ULL,
        120ULL,
        720ULL,
        5040ULL,
        40320ULL,
        362880ULL,
        3628800ULL,
        39916800ULL,
        479001600ULL,
        6227020800ULL,
        87178291200ULL,
        1307674368000ULL,
        20922789888000ULL,
        355687428096000ULL,
        6402373705728000ULL,
        121645100408832000ULL,
        2432902008176640000ULL};
    for (int n = 0; n <= 20; ++n) {
        CHECK_EQ(factorial(n), fact[n]);
    }
    CHECK_THROWS_AS((void)factorial(21), verity::util::VerityError);
    CHECK_THROWS_AS((void)factorial(100), verity::util::VerityError);
    CHECK_THROWS_AS((void)factorial(-1), verity::util::VerityError);
}

TEST_CASE("nCr: known values, symmetry, errors") {
    CHECK_EQ(nCr(5, 2), 10u);
    CHECK_EQ(nCr(52, 5), 2598960u);
    CHECK_EQ(nCr(10, 0), 1u);
    CHECK_EQ(nCr(10, 10), 1u);
    CHECK_EQ(nCr(0, 0), 1u);
    CHECK_EQ(nCr(20, 10), 184756u);
    CHECK_EQ(nCr(40, 20), 137846528820u);
    // large-but-representable coefficient: multiplicative and Pascal methods
    // must agree independently (self-consistent oracle, no magic constant)
    CHECK(binomialIdentityCheck(62, 31));
    CHECK(binomialIdentityCheck(60, 30));
    CHECK_THROWS_AS((void)nCr(5, 6), verity::util::VerityError);
    CHECK_THROWS_AS((void)nCr(-1, 0), verity::util::VerityError);
    CHECK_THROWS_AS((void)nCr(68, 34), verity::util::VerityError);   // C(68,34) ~ 2.8e19 > uint64
}

TEST_CASE("nPr: known values and overflow") {
    CHECK_EQ(nPr(5, 2), 20u);
    CHECK_EQ(nPr(10, 3), 720u);
    CHECK_EQ(nPr(10, 0), 1u);
    CHECK_EQ(nPr(0, 0), 1u);
    CHECK_EQ(nPr(20, 4), 116280u);
    CHECK_THROWS_AS((void)nPr(5, 7), verity::util::VerityError);
    CHECK_THROWS_AS((void)nPr(21, 20), verity::util::VerityError);   // 21!/1! overflows
    // relation: P(n,k) = C(n,k) * k!
    CHECK_EQ(nPr(8, 3), nCr(8, 3) * factorial(3));
}

TEST_CASE("pascalNcr agrees with nCr over the full grid n<=25") {
    for (int n = 0; n <= 25; ++n) {
        for (int k = 0; k <= n; ++k) {
            // both computations are representable on this grid; verify BOTH
            // independently and compare (the frozen contract's identity check)
            CHECK(binomialIdentityCheck(n, k));
            CHECK_EQ(nCr(n, k), pascalNcr(n, k));
        }
    }
    // overflow agreement on the far edge: both must throw for C(68,34)
    CHECK_THROWS_AS((void)pascalNcr(68, 34), verity::util::VerityError);
}

TEST_CASE("binomial theorem identity (a+b)^n over uint64 ring") {
    CHECK(binomialTheoremCheck(1, 1, 10));
    CHECK(binomialTheoremCheck(2, 3, 7));
    CHECK(binomialTheoremCheck(3, 5, 6));
    CHECK(binomialTheoremCheck(7, 2, 9));
    CHECK(binomialTheoremCheck(0, 5, 4));
    CHECK(binomialTheoremCheck(12345, 678, 12));
}
