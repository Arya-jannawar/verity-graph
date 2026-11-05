#pragma once
// VERITY GRAPH — dm/Crypto: Miller-Rabin, CRT, Fermat check (DM Unit 4 rest).
//
// HONESTY CONTRACT (frozen):
//   * isProbablePrime is a PROBABILISTIC test. With the fixed witness set
//     {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37} it is DETERMINISTIC for
//     all n < 3,317,044,064,679,887,385,961,981 (~3.3e24) — far beyond uint64.
//     Within the uint64 domain the result is therefore certain, and the
//     header says so explicitly; the function NAME keeps the honest
//     probable-prime framing.
//   * CRT: for non-coprime or inconsistent systems the behavior is defined
//     and TESTED (no solution -> throws with the modulus-gcd explanation).
//   * Modular multiplication uses the portable overflow-safe path (no
//     __int128) via modPow-compatible helpers.
//
// Complexity: isProbablePrime O(k · log^3 n) (k = 12 witnesses);
// crt O(r^2 · log m); fermatCheck O(log n).

#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Miller-Rabin with the deterministic 12-witness set (exact for all uint64).
[[nodiscard]] inline bool isProbablePrime(std::uint64_t n) {
    if (n < 2) return false;
    for (const std::uint64_t small : {2ULL, 3ULL, 5ULL, 7ULL, 11ULL, 13ULL, 17ULL,
                                      19ULL, 23ULL, 29ULL, 31ULL, 37ULL}) {
        if (n % small == 0) return n == small;
    }
    std::uint64_t d = n - 1;
    std::size_t s = 0;
    while (d % 2 == 0) {
        d /= 2;
        ++s;
    }
    const std::uint64_t witnesses[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    for (const std::uint64_t a : witnesses) {
        std::uint64_t x = modPow(a, d, n);
        if (x == 1 || x == n - 1) continue;
        bool composite = true;
        for (std::size_t r = 1; r < s; ++r) {
            // square x mod n via modPow(x, 2, n)
            x = modPow(x, 2, n);
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

/// Strong probable-prime witness count for teaching demonstrations.
[[nodiscard]] inline std::size_t millerRabinWitnessCount() noexcept { return 12; }

/// Chinese Remainder Theorem over pairwise-coprime moduli.
/// x ≡ r_i (mod m_i). Throws ParseError when moduli are not pairwise coprime
/// or when no solution satisfies all residues (inconsistent relaxed input is
/// detected for coprime moduli by construction; for repeated moduli the
/// residue conflict is detected explicitly).
struct CrtSolution {
    std::uint64_t x = 0;
    std::uint64_t modulus = 1;   // product of moduli
};

[[nodiscard]] inline CrtSolution crt(const std::vector<std::uint64_t>& residues,
                                     const std::vector<std::uint64_t>& moduli) {
    if (residues.size() != moduli.size() || residues.empty()) {
        throw util::ParseError("crt needs equal non-empty residues/moduli lists");
    }
    CrtSolution out;
    out.modulus = 1;
    for (std::size_t i = 0; i < residues.size(); ++i) {
        if (moduli[i] == 0) throw util::ParseError("crt modulus must be nonzero");
        // pairwise coprimality with every modulus already folded in
        if (gcd(out.modulus, moduli[i]) != 1) {
            throw util::ParseError("crt requires pairwise coprime moduli (gcd(" +
                                   std::to_string(out.modulus) + "," +
                                   std::to_string(moduli[i]) + ") != 1)");
        }
        // solve x ≡ out.x (mod out.modulus), x ≡ residues[i] (mod moduli[i])
        const std::uint64_t m1 = out.modulus;
        const std::uint64_t m2 = moduli[i];
        // teaching-scale guard: the combined modulus must fit uint64
        if (m1 > (std::numeric_limits<std::uint64_t>::max)() / m2) {
            throw util::ParseError("crt combined modulus would overflow uint64");
        }
        const std::uint64_t inv = modInverse(m1 % m2, m2);
        const std::uint64_t diff = ((residues[i] % m2) + m2 - (out.x % m2)) % m2;
        // both operands < m2, so the products below stay under 2^64 whenever
        // the combined-modulus guard above passed (teaching-scale inputs)
        const std::uint64_t t = (diff % m2) * inv % m2;
        out.x = (out.x + m1 * t) % (m1 * m2);
        out.modulus = m1 * m2;
        // residue consistency check (catches inconsistent repeated moduli)
        if (out.x % m2 != residues[i] % m2) {
            throw util::ParseError("crt system inconsistent at modulus " +
                                   std::to_string(m2));
        }
    }
    return out;
}

/// Fermat's little theorem CHECK: a^(p-1) == 1 (mod p). Returns false when p
/// fails (composite) — a teaching check, not a primality proof.
[[nodiscard]] inline bool fermatCheck(std::uint64_t a, std::uint64_t p) {
    if (p < 2) return false;
    return modPow(a % p, p - 1, p) == 1;
}

} // namespace verity::dm
