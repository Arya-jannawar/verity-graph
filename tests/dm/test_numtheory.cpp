// VERITY GRAPH — tests/dm/test_numtheory.cpp
// Number theory: extended-Euclid Bézout identity, gcd/lcm/modPow/modInverse
// known values and overflow guards, sieve vs isPrime agreement, nextPrime
// strictly-greater property, Euler totient laws.

#include "doctest/doctest.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

using verity::dm::eulerTotient;
using verity::dm::extendedGcd;
using verity::dm::gcd;
using verity::dm::isPrime;
using verity::dm::lcm;
using verity::dm::modInverse;
using verity::dm::modPow;
using verity::dm::nextPrime;
using verity::dm::sieve;

TEST_CASE("gcd: known values") {
    CHECK_EQ(gcd(12, 18), 6u);
    CHECK_EQ(gcd(240, 46), 2u);
    CHECK_EQ(gcd(17, 5), 1u);
    CHECK_EQ(gcd(0, 7), 7u);
    CHECK_EQ(gcd(7, 0), 7u);
    CHECK_EQ(gcd(0, 0), 0u);
}

TEST_CASE("extendedGcd: Bezout identity a*x + b*y == gcd(a,b)") {
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> cases = {
        {240, 46}, {17, 5}, {0, 7}, {7, 0}, {1, 1}, {1071, 462}, {999999937, 998244353}};
    for (const auto& [a, b] : cases) {
        const auto [g, x, y] = extendedGcd(a, b);
        CHECK_EQ(g, gcd(a, b));
        // verify a*x + b*y == g in signed arithmetic (values small enough)
        const __int128 lhs = static_cast<__int128>(a) * x + static_cast<__int128>(b) * y;
        CHECK_EQ(lhs, static_cast<__int128>(g));
    }
}

TEST_CASE("lcm: known values and overflow guard") {
    CHECK_EQ(lcm(4, 6), 12u);
    CHECK_EQ(lcm(21, 6), 42u);
    CHECK_EQ(lcm(0, 5), 0u);
    CHECK_EQ(lcm(7, 7), 7u);
    // two large coprime odd numbers whose product exceeds uint64
    const std::uint64_t big1 = 9223372036854775801ULL;  // > 2^63, odd
    const std::uint64_t big2 = 9223372036854775617ULL;  // odd, coprime with big1
    CHECK_THROWS_AS((void)lcm(big1, big2), verity::util::VerityError);
}

TEST_CASE("modPow: known values and edge cases") {
    CHECK_EQ(modPow(2, 10, 1000), 24u);            // 1024 mod 1000
    CHECK_EQ(modPow(3, 0, 7), 1u);
    CHECK_EQ(modPow(5, 3, 1), 0u);                 // modulus 1
    CHECK_EQ(modPow(2, 62, 4), 0u);                // 2^62 divisible by 4
    CHECK_THROWS_AS((void)modPow(2, 3, 0), verity::util::VerityError);
    // brute-force oracle for a moderate exponent
    std::uint64_t brute = 1;
    for (int i = 0; i < 12; ++i) brute = (brute * 7) % 1009;
    CHECK_EQ(modPow(7, 12, 1009), brute);
    // Fermat little theorem: a^(p-1) == 1 (mod p) for prime p, gcd(a,p)=1
    CHECK_EQ(modPow(7, 998244352, 998244353), 1u);
    CHECK_EQ(modPow(3, 1000000006, 1000000007), 1u);
    // exponent split law: a^(x+y) == a^x * a^y (mod m)
    const std::uint64_t lhs = modPow(5, 1234 + 567, 1000003);
    const std::uint64_t rhs = (modPow(5, 1234, 1000003) * modPow(5, 567, 1000003)) % 1000003;
    CHECK_EQ(lhs, rhs);
}

TEST_CASE("modInverse: known values and failure paths") {
    CHECK_EQ(modInverse(3, 11), 4u);               // 3*4 = 12 == 1 (mod 11)
    CHECK_EQ(modInverse(10, 17), 12u);
    CHECK_EQ((modInverse(6, 7) * 6) % 7, 1u);
    CHECK_THROWS_AS((void)modInverse(2, 4), verity::util::VerityError);   // gcd != 1
    CHECK_THROWS_AS((void)modInverse(0, 7), verity::util::VerityError);   // inverse of 0
    CHECK_THROWS_AS((void)modInverse(3, 0), verity::util::VerityError);   // bad modulus
}

TEST_CASE("isPrime: edges and known composites") {
    CHECK_FALSE(isPrime(0));
    CHECK_FALSE(isPrime(1));
    CHECK(isPrime(2));
    CHECK(isPrime(3));
    CHECK_FALSE(isPrime(4));
    CHECK_FALSE(isPrime(25));
    CHECK(isPrime(97));
    CHECK(isPrime(7919));
    CHECK(isPrime(2147483647ULL));                 // Mersenne prime 2^31-1
    CHECK_FALSE(isPrime(2147483649ULL));           // 2^31+1 = 3 * 715827883
    CHECK(isPrime(67280421310721ULL));             // factor of 2^64+1
    CHECK_FALSE(isPrime(999999999999ULL));
}

TEST_CASE("sieve agrees with isPrime for all n in [0, 5000]") {
    const auto table = sieve(5000);
    for (std::uint64_t n = 0; n <= 5000; ++n) {
        if (table[n] != isPrime(n)) {
            FAIL("sieve/isPrime mismatch at n = ", static_cast<long long>(n));
        }
    }
    CHECK(table[2]);
    CHECK(table[4999]);
}

TEST_CASE("nextPrime: strictly greater, correct for known values") {
    CHECK_EQ(nextPrime(0), 2u);
    CHECK_EQ(nextPrime(1), 2u);
    CHECK_EQ(nextPrime(2), 3u);
    CHECK_EQ(nextPrime(3), 5u);
    CHECK_EQ(nextPrime(10), 11u);
    CHECK_EQ(nextPrime(64), 67u);
    CHECK_EQ(nextPrime(1000), 1009u);
    for (std::uint64_t n = 2; n < 300; ++n) {
        const std::uint64_t p = nextPrime(n);
        CHECK(p > n);
        CHECK(isPrime(p));
        // minimality: no prime in (n, p)
        for (std::uint64_t q = n + 1; q < p; ++q) {
            CHECK_FALSE(isPrime(q));
        }
    }
}

TEST_CASE("eulerTotient: known values and multiplicative law") {
    CHECK_EQ(eulerTotient(1), 1u);
    CHECK_EQ(eulerTotient(9), 6u);
    CHECK_EQ(eulerTotient(12), 4u);
    CHECK_EQ(eulerTotient(97), 96u);               // prime -> p-1
    CHECK_EQ(eulerTotient(100), 40u);
    // phi(p^k) = p^k - p^(k-1)
    CHECK_EQ(eulerTotient(32), 16u);               // 2^5 -> 2^4
    CHECK_EQ(eulerTotient(243), 162u);             // 3^5 -> 3^5 - 3^4
    // multiplicative for coprime m, n: phi(mn) == phi(m) * phi(n)
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> coprimePairs = {
        {8, 9}, {4, 15}, {7, 10}, {9, 25}, {16, 27}, {5, 14}};
    for (const auto& [m, n] : coprimePairs) {
        CHECK_EQ(eulerTotient(m * n), eulerTotient(m) * eulerTotient(n));
    }
}
