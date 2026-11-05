#pragma once
// VERITY GRAPH — dm/Algebra: semigroup / monoid / group law checkers, cyclic
// groups Z_n and multiplicative groups Z_p*, subgroups, generators, Lagrange
// and homomorphism verification (DM Unit 3, Phase 2; noisy-OR monoid in
// Phase 3 reuses MonoidLaws).
//
// Law checkers are EXHAUSTIVE over the supplied element set (small domains
// only — O(|S|^k) in the law's arity; callers keep domains ≤ ~64 elements).
//
// Complexity summary:
//   SemigroupLaws::check      O(n^3)
//   MonoidLaws::check         O(n^3) + O(n^2) identity scan
//   GroupLaws::check          O(n^3) + O(n^2) inverses
//   ZnPlus / ZpMul            O(n) construction
//   cyclicGenerators          O(n^2)
//   subgroupOfCyclic          O(n^2)
//   lagrangeHolds             O(n^2)
//   homomorphismHolds         O(n^2)

#include <cstdint>
#include <vector>

#include "verity/dm/NumTheory.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

/// Exhaustive associativity check over all triples.
template <class T, class Op>
[[nodiscard]] bool isAssociative(const std::vector<T>& set, Op op) {
    for (const T& a : set) {
        for (const T& b : set) {
            for (const T& c : set) {
                if (op(op(a, b), c) != op(a, op(b, c))) return false;
            }
        }
    }
    return true;
}

/// Semigroup: associativity. Returns false rather than throwing.
template <class T, class Op>
[[nodiscard]] bool semigroupHolds(const std::vector<T>& set, Op op) {
    if (set.empty()) return false;   // a semigroup has at least one element here
    return isAssociative(set, op);
}

/// Monoid: associativity + two-sided identity. Returns identity via `identity`
/// when true (0-constructible T required for the scan). MonoidLaws::check
/// verifies a CLAIMED identity; findIdentity searches for one.
template <class T, class Op>
[[nodiscard]] bool monoidHolds(const std::vector<T>& set, Op op, const T& identity) {
    if (!semigroupHolds(set, op)) return false;
    for (const T& a : set) {
        if (!(op(identity, a) == a) || !(op(a, identity) == a)) return false;
    }
    return true;
}

template <class T, class Op>
[[nodiscard]] bool findIdentity(const std::vector<T>& set, Op op, T& outIdentity) {
    for (const T& candidate : set) {
        bool ok = true;
        for (const T& a : set) {
            if (!(op(candidate, a) == a) || !(op(a, candidate) == a)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            outIdentity = candidate;
            return true;
        }
    }
    return false;
}

/// Group: monoid + two-sided inverses for every element.
template <class T, class Op>
[[nodiscard]] bool groupHolds(const std::vector<T>& set, Op op, const T& identity) {
    if (!monoidHolds(set, op, identity)) return false;
    for (const T& a : set) {
        bool hasInverse = false;
        for (const T& b : set) {
            if (op(a, b) == identity && op(b, a) == identity) {
                hasInverse = true;
                break;
            }
        }
        if (!hasInverse) return false;
    }
    return true;
}

/// Subgroup check (subset of a known group): membership + closure + identity
/// + inverses.
template <class T, class Op>
[[nodiscard]] bool subgroupHolds(const std::vector<T>& groupSet, Op op, const T& identity,
                                 const std::vector<T>& subset) {
    if (subset.empty()) return false;
    // membership: every subset element must be a group element
    for (const T& a : subset) {
        bool member = false;
        for (const T& g : groupSet) {
            if (g == a) { member = true; break; }
        }
        if (!member) return false;
    }
    // closure
    for (const T& a : subset) {
        for (const T& b : subset) {
            const T product = op(a, b);
            bool closed = false;
            for (const T& g : subset) {
                if (g == product) { closed = true; break; }
            }
            if (!closed) return false;
        }
    }
    // identity
    bool hasIdentity = false;
    for (const T& a : subset) {
        if (a == identity) hasIdentity = true;
    }
    if (!hasIdentity) return false;
    // inverses
    for (const T& a : subset) {
        bool inv = false;
        for (const T& b : subset) {
            if (op(a, b) == identity) { inv = true; break; }
        }
        if (!inv) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Concrete groups
// ---------------------------------------------------------------------------

/// Additive cyclic group (Z_n, +): elements 0..n-1.
struct ZnPlus {
    std::uint64_t n;

    [[nodiscard]] std::vector<std::uint64_t> elements() const {
        std::vector<std::uint64_t> out;
        out.reserve(n);
        for (std::uint64_t i = 0; i < n; ++i) out.push_back(i);
        return out;
    }
    [[nodiscard]] std::uint64_t op(std::uint64_t a, std::uint64_t b) const { return (a + b) % n; }
    [[nodiscard]] std::uint64_t identity() const { return 0; }
    /// elements k with gcd(k, n) == 1 generate the whole group.
    [[nodiscard]] std::vector<std::uint64_t> cyclicGenerators() const {
        std::vector<std::uint64_t> out;
        for (std::uint64_t k = 1; k < n; ++k) {
            if (gcd(k, n) == 1) out.push_back(k);
        }
        return out;
    }
    /// subgroup generated by k: { (k·i) mod n : i = 0.. } — order n / gcd(n,k).
    [[nodiscard]] std::vector<std::uint64_t> subgroupGeneratedBy(std::uint64_t k) const {
        std::vector<std::uint64_t> out{0};
        std::uint64_t cur = k % n;
        while (cur != 0) {
            out.push_back(cur);
            cur = (cur + k) % n;
        }
        return out;
    }
};

/// Multiplicative group (Z_p*, ×) for prime p: elements 1..p-1.
/// Precondition: p < 65536 so that a*b fits in uint64 without overflow
/// (teaching-scale groups; Phase-4 modular arithmetic handles large primes).
struct ZpMul {
    std::uint64_t p;   // MUST be prime (verified in ctor)

    explicit ZpMul(std::uint64_t prime) : p(prime) {
        if (!isPrime(prime)) {
            throw util::ParseError("ZpMul requires a prime modulus, got " + std::to_string(prime));
        }
        if (prime >= 65536) {
            throw util::ParseError("ZpMul teaching cap: prime must be < 65536");
        }
    }

    [[nodiscard]] std::vector<std::uint64_t> elements() const {
        std::vector<std::uint64_t> out;
        out.reserve(p - 1);
        for (std::uint64_t i = 1; i < p; ++i) out.push_back(i);
        return out;
    }
    [[nodiscard]] std::uint64_t op(std::uint64_t a, std::uint64_t b) const {
        return (a * b) % p;
    }
    [[nodiscard]] std::uint64_t identity() const { return 1; }
    [[nodiscard]] std::vector<std::uint64_t> cyclicGenerators() const {
        // Z_p* is cyclic: g is a generator iff ord(g) == p-1
        std::vector<std::uint64_t> out;
        for (std::uint64_t g = 1; g < p; ++g) {
            std::uint64_t cur = g, order = 1;
            while (cur != 1) {
                cur = (cur * g) % p;
                ++order;
            }
            if (order == p - 1) out.push_back(g);
        }
        return out;
    }
};

/// Lagrange's theorem verification: subgroup order divides group order.
[[nodiscard]] inline bool lagrangeHolds(std::size_t groupOrder, std::size_t subgroupOrder) {
    return groupOrder % subgroupOrder == 0;
}

/// Homomorphism check between two groups: f(xy) == f(x)f(y) for ALL pairs.
/// `fOk` marks elements of G mapped outside H's set (caller folds membership
/// into the check result — a map into H only).
template <class S, class T, class OpS, class OpT, class F>
[[nodiscard]] bool homomorphismHolds(const std::vector<S>& domain, OpS opS,
                                     const std::vector<T>& codomain, OpT opT, F f) {
    // every image must land inside the codomain set
    for (const S& x : domain) {
        bool inside = false;
        for (const T& h : codomain) {
            if (f(x) == h) { inside = true; break; }
        }
        if (!inside) return false;
    }
    for (const S& x : domain) {
        for (const S& y : domain) {
            const T left = f(opS(x, y));
            const T right = opT(f(x), f(y));
            if (!(left == right)) return false;
        }
    }
    return true;
}

} // namespace verity::dm
