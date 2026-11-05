#pragma once
// VERITY GRAPH — ds/Levenshtein: edit-distance DP (Phase 2). EntityResolver
// uses the normalized distance for candidate surname matching; the DP table
// is exposed for the required table-print test and DM Lab display.
//
// Complexity: O(m·n) time, O(m·n) memory (full table kept for teaching).

#include <cstddef>
#include <string>
#include <vector>

namespace verity::ds {

/// Edit distance (insert/delete/substitute, all cost 1).
/// Returns the distance; `table` optionally receives the full DP matrix
/// ((m+1) x (n+1), row-major, table[i][j] = distance(a[0..i), b[0..j)).
[[nodiscard]] inline std::size_t levenshtein(const std::string& a, const std::string& b,
                                             std::vector<std::vector<std::size_t>>* table = nullptr) {
    const std::size_t m = a.size(), n = b.size();
    std::vector<std::vector<std::size_t>> dp(m + 1, std::vector<std::size_t>(n + 1, 0));
    for (std::size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (std::size_t j = 0; j <= n; ++j) dp[0][j] = j;
    for (std::size_t i = 1; i <= m; ++i) {
        for (std::size_t j = 1; j <= n; ++j) {
            const std::size_t sub = dp[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0u : 1u);
            const std::size_t del = dp[i - 1][j] + 1;
            const std::size_t ins = dp[i][j - 1] + 1;
            dp[i][j] = sub < del ? (sub < ins ? sub : ins) : (del < ins ? del : ins);
        }
    }
    if (table != nullptr) *table = dp;
    return dp[m][n];
}

} // namespace verity::ds
