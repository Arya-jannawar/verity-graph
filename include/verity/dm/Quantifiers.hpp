#pragma once
// VERITY GRAPH — dm/Quantifiers: finite-domain predicate logic (DM Unit 1).
//
// forall / exists over EXPLICIT finite domains with boolean predicates, plus
// nested quantifiers and the quantifier negation laws verified over test
// domains: !(∀x p(x)) == ∃x !p(x)  and  !(∃x p(x)) == ∀x !p(x).
//
// Complexity: O(|domain|) per quantifier; nested O(|d1|·|d2|).

#include <string>
#include <vector>

namespace verity::dm {

/// ∀x ∈ domain : p(x)
template <class T, class Pred>
[[nodiscard]] bool forAll(const std::vector<T>& domain, Pred p) {
    for (const T& x : domain) {
        if (!p(x)) return false;
    }
    return true;
}

/// ∃x ∈ domain : p(x)
template <class T, class Pred>
[[nodiscard]] bool exists(const std::vector<T>& domain, Pred p) {
    for (const T& x : domain) {
        if (p(x)) return true;
    }
    return false;
}

/// ∃!x ∈ domain : p(x) — exactly one witness.
template <class T, class Pred>
[[nodiscard]] bool existsUnique(const std::vector<T>& domain, Pred p) {
    std::size_t witnesses = 0;
    for (const T& x : domain) {
        if (p(x)) ++witnesses;
    }
    return witnesses == 1;
}

} // namespace verity::dm
