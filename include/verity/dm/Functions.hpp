#pragma once
// VERITY GRAPH — dm/Functions: finite functions and their properties
// (DM Unit 1, Phase 2).
//
// A function is a set of pairs (a,b) with every a occurring at most once.
// The domain/codomain are supplied explicitly so surjectivity is meaningful.
// EntityResolver uses isTotalSurjective to verify the mention→canonical
// mapping is total and onto the canonical-entity set.
//
// Complexity: all checks O(n) or O(n log n) via HashTable.

#include <utility>
#include <vector>

#include "verity/ds/HashTable.hpp"
#include "verity/dm/FiniteSet.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

template <class A, class B>
class FiniteFunction {
public:
    FiniteFunction(FiniteSet<A> domain, FiniteSet<B> codomain,
                   std::vector<std::pair<A, B>> mapping)
        : domain_(std::move(domain)),
          codomain_(std::move(codomain)),
          mapping_(std::move(mapping)) {
        for (const auto& [a, b] : mapping_) {
            if (!domain_.contains(a)) {
                throw util::ParseError("mapping references element outside domain");
            }
            if (!codomain_.contains(b)) {
                throw util::ParseError("mapping references element outside codomain");
            }
            if (!image_.insert(a, b)) {
                throw util::ParseError("function maps an element twice");
            }
        }
    }

    [[nodiscard]] const B* apply(const A& a) const { return image_.get(a); }

    /// every domain element has an image.
    [[nodiscard]] bool isTotal() const noexcept { return mapping_.size() == domain_.size(); }

    /// distinct a ↦ distinct b.
    [[nodiscard]] bool isInjective() const {
        FiniteSet<B> images;
        for (const auto& [a, b] : mapping_) {
            (void)a;
            images.insert(b);
        }
        return images.size() == mapping_.size();
    }

    /// every codomain element is hit.
    [[nodiscard]] bool isSurjective() const {
        FiniteSet<B> images;
        for (const auto& [a, b] : mapping_) {
            (void)a;
            images.insert(b);
        }
        return images.size() == codomain_.size();
    }

    [[nodiscard]] bool isBijective() const { return isTotal() && isInjective() && isSurjective(); }

    /// Total AND onto the codomain (the resolver's "verification" property).
    [[nodiscard]] bool isTotalSurjective() const { return isTotal() && isSurjective(); }

    /// g∘f over the same codomain/domain family; throws when not defined.
    [[nodiscard]] static FiniteFunction compose(const FiniteFunction& g, const FiniteFunction& f) {
        std::vector<std::pair<A, B>> out;
        for (const auto& [a, mid] : f.mapping_) {
            const B* next = g.apply(mid);
            if (next == nullptr) {
                throw util::VerityError("composition undefined: f's image outside g's domain");
            }
            out.emplace_back(a, *next);
        }
        return FiniteFunction(f.domain_, g.codomain_, std::move(out));
    }

    /// Inverse function; throws when f is not bijective.
    [[nodiscard]] FiniteFunction inverse() const {
        if (!isBijective()) {
            throw util::VerityError("inverse requires a bijective function");
        }
        std::vector<std::pair<A, B>> flipped;
        flipped.reserve(mapping_.size());
        for (const auto& [a, b] : mapping_) flipped.emplace_back(b, a);
        return FiniteFunction(codomain_, domain_, std::move(flipped));
    }

    [[nodiscard]] const std::vector<std::pair<A, B>>& mapping() const noexcept { return mapping_; }
    [[nodiscard]] const FiniteSet<A>& domain() const noexcept { return domain_; }
    [[nodiscard]] const FiniteSet<B>& codomain() const noexcept { return codomain_; }

private:
    FiniteSet<A> domain_;
    FiniteSet<B> codomain_;
    std::vector<std::pair<A, B>> mapping_;
    ds::HashTable<A, B> image_;
};

} // namespace verity::dm
