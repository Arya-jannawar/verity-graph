#pragma once
// VERITY GRAPH — dm/NumTheory: elementary number theory (DM Unit 4 core).
//
//     gcd, extendedGcd, lcm (overflow-guarded), modPow, modInverse,
//     isPrime (trial division), sieve, nextPrime, eulerTotient.
//
// nextPrime powers HashTable prime sizing (phase-1 requirement).
// All functions are pure and header-only. Overflow behavior is documented
// per-function; "overflow-guarded" means the function *throws ParseError*
// rather than wrapping around silently.
//
// Complexity summary:
//   gcd/extendedGcd  O(log min(a,b))
//   lcm              O(log min(a,b))
//   modPow           O(log e) multiplications
//   modInverse       O(log m)
//   isPrime          O(sqrt(n))
//   sieve(n)         O(n log log n) time, O(n) memory
//   nextPrime        O(k * sqrt(n)) with small expected k
//   eulerTotient     O(sqrt(n))

#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Greatest common divisor (Euclid). gcd(0,0) == 0.
[[nodiscard]] inline std::uint64_t gcd(std::uint64_t a, std::uint64_t b) noexcept {
    while (b != 0) {
        const std::uint64_t t = a % b;
        a = b;
        b = t;
    }
    return a;
}

/// Extended Euclid: returns (g, x, y) with a*x + b*y == g == gcd(a,b).
/// Works for a == 0 or b == 0. Complexity O(log min(a,b)).
[[nodiscard]] inline std::tuple<std::uint64_t, std::int64_t, std::int64_t>
extendedGcd(std::uint64_t a, std::uint64_t b) noexcept {
    std::uint64_t r_old = a, r = b;  // remainders
    std::int64_t s_old = 1, s = 0;   // coefficient of a
    std::int64_t t_old = 0, t = 1;   // coefficient of b
    while (r != 0) {
        const std::uint64_t q = r_old / r;
        const std::uint64_t nr = r_old - q * r;
        r_old = r;
        r = nr;
        const std::int64_t ns = s_old - static_cast<std::int64_t>(q) * s;
        s_old = s;
        s = ns;
        const std::int64_t nt = t_old - static_cast<std::int64_t>(q) * t;
        t_old = t;
        t = nt;
    }
    return {r_old, s_old, t_old};
}

/// Least common multiple, overflow-guarded: throws ParseError when lcm
/// exceeds the uint64 range instead of wrapping.
[[nodiscard]] inline std::uint64_t lcm(std::uint64_t a, std::uint64_t b) {
    if (a == 0 || b == 0) return 0;
    const std::uint64_t g = gcd(a, b);
    const std::uint64_t half = a / g;
    if (half != 0 && b > (std::numeric_limits<std::uint64_t>::max)() / half) {
        throw util::ParseError("lcm overflow for " + std::to_string(a) + ", " + std::to_string(b));
    }
    return half * b;
}

/// Fast modular exponentiation (binary method). Handles modulus 1 (result 0).
/// The inner multiply uses the portable subtraction form that never
/// overflows uint64 (no __int128 dependency: the MSVC-targeted clang link
/// lacks the __umodti3 128-bit modulo helper).
/// Complexity O(log e).
[[nodiscard]] inline std::uint64_t modPow(std::uint64_t base, std::uint64_t exp, std::uint64_t mod) {
    if (mod == 0) throw util::ParseError("modPow modulus must be nonzero");
    if (mod == 1) return 0;
    auto mulmod = [](std::uint64_t x, std::uint64_t y, std::uint64_t m) -> std::uint64_t {
        std::uint64_t acc = 0;
        x %= m;
        while (y > 0) {
            if (y & 1ULL) {
                // acc = (acc + x) mod m without overflow:
                acc = (m - acc) > x ? acc + x : x - (m - acc);
            }
            // x = (2x) mod m without overflow:
            x = (m - x) > x ? x + x : x - (m - x);
            y >>= 1;
        }
        return acc;
    };
    std::uint64_t result = 1;
    base %= mod;
    while (exp > 0) {
        if (exp & 1ULL) result = mulmod(result, base, mod);
        base = mulmod(base, base, mod);
        exp >>= 1;
    }
    return result;
}

/// Modular inverse via extended Euclid. Throws ParseError when gcd(a,m) != 1.
/// Requires m >= 1; the inverse of 0 is undefined (throws).
[[nodiscard]] inline std::uint64_t modInverse(std::uint64_t a, std::uint64_t m) {
    if (m == 0) throw util::ParseError("modInverse modulus must be nonzero");
    if (a == 0) throw util::ParseError("modInverse of 0 does not exist");
    const auto [g, x, y] = extendedGcd(a % m, m);
    (void)y;
    if (g != 1) {
        throw util::ParseError("modInverse does not exist: gcd(" + std::to_string(a) +
                               "," + std::to_string(m) + ") = " + std::to_string(g));
    }
    std::int64_t inv = x % static_cast<std::int64_t>(m);
    if (inv < 0) inv += static_cast<std::int64_t>(m);
    return static_cast<std::uint64_t>(inv);
}

/// Deterministic primality by trial division. isPrime(0|1) == false.
/// Complexity O(sqrt(n)).
[[nodiscard]] inline bool isPrime(std::uint64_t n) noexcept {
    if (n < 2) return false;
    if (n < 4) return true;         // 2, 3
    if (n % 2 == 0) return false;
    if (n % 3 == 0) return false;
    for (std::uint64_t i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0) return false;
    }
    return true;
}

/// Sieve of Eratosthenes: primality flags for [0, n]. O(n log log n).
[[nodiscard]] inline std::vector<bool> sieve(std::uint64_t n) {
    std::vector<bool> is_prime(n + 1, true);
    is_prime[0] = false;                       // index 0 always exists (n+1 >= 1)
    if (n >= 1) { is_prime[1] = false; }
    for (std::uint64_t i = 2; i * i <= n; ++i) {
        if (is_prime[i]) {
            for (std::uint64_t j = i * i; j <= n; j += i) is_prime[j] = false;
        }
    }
    return is_prime;
}

/// Smallest prime STRICTLY GREATER than n. Used by HashTable to round a
/// required capacity up to a prime, so the strict form avoids returning the
/// same size. nextPrime(0) == 2, nextPrime(1) == 2. Complexity O(k*sqrt(n)).
[[nodiscard]] inline std::uint64_t nextPrime(std::uint64_t n) {
    std::uint64_t candidate = (n < 2) ? 2 : n + 1;
    if (candidate > 2 && candidate % 2 == 0) ++candidate;
    while (!isPrime(candidate)) candidate += 2;
    return candidate;
}

/// Euler's totient φ(n): count of k in [1,n] with gcd(k,n) == 1.
/// Complexity O(sqrt(n)).
[[nodiscard]] inline std::uint64_t eulerTotient(std::uint64_t n) {
    if (n == 0) return 0;
    std::uint64_t result = n;
    std::uint64_t m = n;
    for (std::uint64_t p = 2; p * p <= m; ++p) {
        if (m % p == 0) {
            while (m % p == 0) m /= p;
            result -= result / p;
        }
    }
    if (m > 1) result -= result / m;
    return result;
}

} // namespace verity::dm
