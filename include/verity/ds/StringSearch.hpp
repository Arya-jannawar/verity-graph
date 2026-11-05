#pragma once
// VERITY GRAPH — ds/StringSearch: Rabin-Karp substring search with a rolling
// hash mod a prime (Phase 3). Used by span re-verification and excerpt lookup
// in the UI pipeline.
//
// The rolling hash uses verity::dm's modular arithmetic (base 256, mod the
// prime 1000000007). Hash equality is always VERIFIED character-by-character
// (no false positives), matching the brief's exact-span discipline.
//
// Complexity: expected O(n + m); worst case O(n·m) (verified collisions).

#include <cstddef>
#include <string>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::ds {

/// First index of `needle` in `haystack` via Rabin-Karp (verified matches
/// only); npos when absent.
[[nodiscard]] inline std::size_t rabinKarpFind(const std::string& haystack,
                                               const std::string& needle) {
    constexpr std::uint64_t kMod = 1000000007ULL;
    constexpr std::uint64_t kBase = 256;
    const std::size_t n = haystack.size(), m = needle.size();
    if (m == 0 || m > n) return m == 0 ? 0 : static_cast<std::size_t>(-1);

    std::uint64_t hashNeedle = 0, hashWindow = 0, highBase = 1;
    for (std::size_t i = 0; i < m; ++i) {
        hashNeedle = (hashNeedle * kBase + static_cast<unsigned char>(needle[i])) % kMod;
        hashWindow = (hashWindow * kBase + static_cast<unsigned char>(haystack[i])) % kMod;
        if (i < m - 1) highBase = (highBase * kBase) % kMod;
    }
    for (std::size_t i = 0; i + m <= n; ++i) {
        if (hashWindow == hashNeedle) {
            // verified match: no silent hash false positives
            if (haystack.compare(i, m, needle) == 0) return i;
        }
        if (i + m < n) {
            // roll: remove haystack[i], add haystack[i+m]
            const std::uint64_t removed =
                (static_cast<std::uint64_t>(static_cast<unsigned char>(haystack[i])) * highBase) % kMod;
            hashWindow = (hashWindow + kMod - removed) % kMod;
            hashWindow = (hashWindow * kBase + static_cast<unsigned char>(haystack[i + m])) % kMod;
        }
    }
    return static_cast<std::size_t>(-1);
}

/// All verified occurrences of `needle` in `haystack`, ascending.
[[nodiscard]] inline std::vector<std::size_t> rabinKarpFindAll(const std::string& haystack,
                                                               const std::string& needle) {
    std::vector<std::size_t> out;
    std::size_t pos = rabinKarpFind(haystack, needle);
    while (pos != static_cast<std::size_t>(-1)) {
        out.push_back(pos);
        if (pos + 1 >= haystack.size()) break;
        pos = rabinKarpFind(haystack.substr(pos + 1), needle);
        if (pos != static_cast<std::size_t>(-1)) pos += out.back() + 1;
    }
    return out;
}

} // namespace verity::ds
