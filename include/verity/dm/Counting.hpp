#pragma once
// VERITY GRAPH — Counting: factorial / nCr / nPr / Pascal / binomial theorem
// (DM Unit 2 basics, Phase 1).
//
// Overflow policy: uint64 results are exact while representable; anything
// beyond throws ParseError instead of wrapping (factorial at 21!, nPr when the
// product overflows, nCr likewise via the Pascal check).
//
// Complexity summary:
//   factorial            O(n) multiplications, exact for n<=20
//   nPr                  O(k) multiplications
//   nCr multiplicative   O(k) mult+div, guarded
//   pascalNcr            O(n) additions (single row DP)
//   binomialTheoremCheck O(n * nCr-terms) over uint64 ring (wrapping mod 2^64,
//                        documented teaching check)

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Exact factorial for n<=20; throws ParseError for n>=21 (21! overflows
/// uint64). factorial(0) == 1.
[[nodiscard]] inline std::uint64_t factorial(int n) {
    if (n < 0) throw util::ParseError("factorial of negative " + std::to_string(n));
    if (n >= 21) throw util::ParseError("factorial overflow for n>20 (n=" + std::to_string(n) + ")");
    std::uint64_t r = 1;
    for (int i = 2; i <= n; ++i) r *= static_cast<std::uint64_t>(i);
    return r;
}

/// Permutations P(n,k) = n!/(n-k)!: multiplicative, divide-free, guarded.
/// Throws ParseError for k>n or on uint64 overflow.
[[nodiscard]] inline std::uint64_t nPr(int n, int k) {
    if (n < 0 || k < 0) throw util::ParseError("nPr needs nonnegative arguments");
    if (k > n) throw util::ParseError("nPr: k>n (" + std::to_string(k) + ">" + std::to_string(n) + ")");
    std::uint64_t r = 1;
    for (int i = 0; i < k; ++i) {
        const std::uint64_t factor = static_cast<std::uint64_t>(n - i);
        if (r > (std::numeric_limits<std::uint64_t>::max)() / factor) {
            throw util::ParseError("nPr overflow for (" + std::to_string(n) + "," + std::to_string(k) + ")");
        }
        r *= factor;
    }
    return r;
}

/// Combinations C(n,k): multiplicative method with divide-at-each-step to
/// keep intermediates minimal; throws ParseError for k>n or uint64 overflow.
/// C(n,0) == C(n,n) == 1.
[[nodiscard]] inline std::uint64_t nCr(int n, int k) {
    if (n < 0 || k < 0) throw util::ParseError("nCr needs nonnegative arguments");
    if (k > n) throw util::ParseError("nCr: k>n (" + std::to_string(k) + ">" + std::to_string(n) + ")");
    const int kk = (k > n - k) ? n - k : k;   // symmetry C(n,k)=C(n,n-k)
    std::uint64_t r = 1;
    for (int i = 1; i <= kk; ++i) {
        const std::uint64_t numerator = static_cast<std::uint64_t>(n - kk + i);
        // r is C(n-kk+i-1, i-1); r * numerator / i is exact and equals
        // C(n-kk+i, i). Reducing by gcd(r,i) first keeps intermediates small.
        const std::uint64_t g = gcd(r, static_cast<std::uint64_t>(i));
        const std::uint64_t rDiv = r / g;
        const std::uint64_t iDiv = static_cast<std::uint64_t>(i) / g;
        if (rDiv > (std::numeric_limits<std::uint64_t>::max)() / numerator) {
            throw util::ParseError("nCr overflow for (" + std::to_string(n) + "," + std::to_string(k) + ")");
        }
        const std::uint64_t prod = rDiv * numerator;
        r = prod / iDiv;   // exact: binomial coefficients are integers
    }
    return r;
}

/// Pascal-triangle single-row DP for C(n,k): addition-only construction.
/// row[j] holds C(i, j) after processing row i; overflow throws ParseError.
/// Same representability domain as nCr.
[[nodiscard]] inline std::uint64_t pascalNcr(int n, int k) {
    if (n < 0 || k < 0) throw util::ParseError("pascalNcr needs nonnegative arguments");
    if (k > n) throw util::ParseError("pascalNcr: k>n");
    std::vector<std::uint64_t> row(static_cast<std::size_t>(k) + 1, 0);
    row[0] = 1;   // C(0, 0) = 1, C(0, j>0) = 0
    for (int i = 1; i <= n; ++i) {
        const int top = (i < k) ? i : k;   // columns beyond i and k stay 0
        for (int j = top; j >= 1; --j) {
            const std::uint64_t sum = row[static_cast<std::size_t>(j - 1)] +
                                      row[static_cast<std::size_t>(j)];
            if (sum < row[static_cast<std::size_t>(j)]) {
                throw util::ParseError("pascalNcr overflow at row " + std::to_string(i));
            }
            row[static_cast<std::size_t>(j)] = sum;
        }
    }
    return row[static_cast<std::size_t>(k)];
}

/// Independent-computation identity check: nCr(n,k) == pascalNcr(n,k).
/// Returns false when either computation overflows (documented behavior:
/// the identity is only asserted on the representable domain).
[[nodiscard]] inline bool binomialIdentityCheck(int n, int k) {
    std::uint64_t a = 0, b = 0;
    bool okA = true, okB = true;
    try {
        a = nCr(n, k);
    } catch (const util::VerityError&) {
        okA = false;
    }
    try {
        b = pascalNcr(n, k);
    } catch (const util::VerityError&) {
        okB = false;
    }
    if (!okA || !okB) return false;   // both must be representable to compare
    return a == b;
}

/// Binomial theorem check over the wrapping uint64 ring (mod 2^64):
/// sum_k C(n,k) * a^k * b^(n-k)  ==  (a+b)^n, both sides mod 2^64.
/// Wrapping is intentional and documented (teaching identity check); powers
/// are plain iterated products, which wrap naturally in uint64 arithmetic.
[[nodiscard]] inline bool binomialTheoremCheck(std::uint64_t a, std::uint64_t b, int n) {
    if (n < 0) return false;
    auto powerWrap = [](std::uint64_t base, int exp) noexcept {
        std::uint64_t out = 1;
        for (int i = 0; i < exp; ++i) out *= base;   // wraps mod 2^64
        return out;
    };
    std::uint64_t lhs = 0;
    for (int k = 0; k <= n; ++k) {
        const std::uint64_t coeff = nCr(n, k);   // exact for n<=20; throws beyond
        lhs += coeff * powerWrap(a, k) * powerWrap(b, n - k);
    }
    std::uint64_t rhs = 1;
    for (int i = 0; i < n; ++i) rhs *= (a + b);
    return lhs == rhs;
}

} // namespace verity::dm
