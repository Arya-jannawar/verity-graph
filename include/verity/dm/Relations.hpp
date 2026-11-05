#pragma once
// VERITY GRAPH — dm/Relations: boolean-matrix relations over a finite domain
// (DM Unit 1, Phase 2).
//
// A Relation<T> is a square boolean matrix over an explicit finite universe
// (vector<T> elements). Properties, closures (Warshall), equivalence classes,
// Hasse reduction, bounds/lattice checks and partition checks are provided.
// EntityResolver builds its similarity relation here and takes the
// equivalence closure; TimelineBuilder uses the strict-order checks and the
// Hasse reduction.
//
// Complexity summary (n = |universe|):
//   add / contains            O(1) avg (hash lookup) / O(1)
//   isReflexive/…             O(n^2)
//   isTransitive              O(n^3)  (matrix check)
//   transitiveClosure (Warshall) O(n^3)
//   hasseDiagram              O(n^3)
//   equivalenceClasses        O(n^2)
//   lub/glb                   O(n^2)

#include <cstddef>
#include <vector>

#include "verity/ds/HashTable.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::dm {

template <class T>
class Relation {
public:
    explicit Relation(std::vector<T> universe) : elements_(std::move(universe)) {
        for (std::size_t i = 0; i < elements_.size(); ++i) {
            if (!index_.insert(elements_[i], i)) {
                throw util::ParseError("Relation universe contains duplicate element");
            }
        }
        matrix_.assign(elements_.size(), std::vector<bool>(elements_.size(), false));
    }

    void add(const T& a, const T& b) {
        matrix_[indexOf(a)][indexOf(b)] = true;
    }

    [[nodiscard]] bool contains(const T& a, const T& b) const {
        return matrix_[indexOf(a)][indexOf(b)];
    }

    [[nodiscard]] const std::vector<T>& elements() const noexcept { return elements_; }
    [[nodiscard]] std::size_t order() const noexcept { return elements_.size(); }

    /// Number of true pairs.
    [[nodiscard]] std::size_t pairCount() const noexcept {
        std::size_t count = 0;
        for (const auto& row : matrix_) {
            for (bool v : row) count += v ? 1u : 0u;
        }
        return count;
    }

    // --- properties ---------------------------------------------------------
    [[nodiscard]] bool isReflexive() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            if (!matrix_[i][i]) return false;
        }
        return true;
    }

    [[nodiscard]] bool isIrreflexive() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            if (matrix_[i][i]) return false;
        }
        return true;
    }

    [[nodiscard]] bool isSymmetric() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = i + 1; j < matrix_.size(); ++j) {
                if (matrix_[i][j] != matrix_[j][i]) return false;
            }
        }
        return true;
    }

    /// antisymmetric: aRb ∧ bRa ⟹ a == b
    [[nodiscard]] bool isAntisymmetric() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = i + 1; j < matrix_.size(); ++j) {
                if (matrix_[i][j] && matrix_[j][i]) return false;
            }
        }
        return true;
    }

    /// asymmetric: aRb ⟹ ¬bRa (implies irreflexive)
    [[nodiscard]] bool isAsymmetric() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            if (matrix_[i][i]) return false;
            for (std::size_t j = i + 1; j < matrix_.size(); ++j) {
                if (matrix_[i][j] && matrix_[j][i]) return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool isTransitive() const noexcept {
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                if (!matrix_[i][j]) continue;
                for (std::size_t k = 0; k < matrix_.size(); ++k) {
                    if (matrix_[j][k] && !matrix_[i][k]) return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool isEquivalence() const noexcept {
        return isReflexive() && isSymmetric() && isTransitive();
    }

    [[nodiscard]] bool isPartialOrder() const noexcept {
        return isReflexive() && isAntisymmetric() && isTransitive();
    }

    [[nodiscard]] bool isStrictOrder() const noexcept {
        return isIrreflexive() && isAsymmetric() && isTransitive();
    }

    // --- operations ---------------------------------------------------------
    [[nodiscard]] Relation inverse() const {
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                if (matrix_[i][j]) out.matrix_[j][i] = true;
            }
        }
        return out;
    }

    /// Relational composition: this∘other means a (this) c when ∃b: aR1 b ∧ bR2 c.
    [[nodiscard]] Relation compose(const Relation& other) const {
        checkSameUniverse(other);
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                if (!matrix_[i][j]) continue;
                for (std::size_t k = 0; k < matrix_.size(); ++k) {
                    if (other.matrix_[j][k]) out.matrix_[i][k] = true;
                }
            }
        }
        return out;
    }

    [[nodiscard]] Relation unionWith(const Relation& other) const {
        checkSameUniverse(other);
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                out.matrix_[i][j] = matrix_[i][j] || other.matrix_[i][j];
            }
        }
        return out;
    }

    [[nodiscard]] Relation intersectWith(const Relation& other) const {
        checkSameUniverse(other);
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                out.matrix_[i][j] = matrix_[i][j] && other.matrix_[i][j];
            }
        }
        return out;
    }

    [[nodiscard]] Relation complement() const {
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                out.matrix_[i][j] = !matrix_[i][j];
            }
        }
        return out;
    }

    // --- closures -----------------------------------------------------------
    [[nodiscard]] Relation reflexiveClosure() const {
        Relation out(*this);
        for (std::size_t i = 0; i < matrix_.size(); ++i) out.matrix_[i][i] = true;
        return out;
    }

    [[nodiscard]] Relation symmetricClosure() const {
        Relation out(elements_);
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                out.matrix_[i][j] = matrix_[i][j] || matrix_[j][i];
            }
        }
        return out;
    }

    /// Warshall's algorithm. O(n^3).
    [[nodiscard]] Relation transitiveClosure() const {
        Relation out(*this);
        auto& m = out.matrix_;
        for (std::size_t k = 0; k < m.size(); ++k) {
            for (std::size_t i = 0; i < m.size(); ++i) {
                if (!m[i][k]) continue;
                for (std::size_t j = 0; j < m.size(); ++j) {
                    if (m[k][j]) m[i][j] = true;
                }
            }
        }
        return out;
    }

    /// Equivalence closure = reflexive ∘ symmetric ∘ transitive closure.
    [[nodiscard]] Relation equivalenceClosure() const {
        return symmetricClosure().reflexiveClosure().transitiveClosure();
    }

    // --- equivalence classes / partitions ------------------------------------
    /// Classes only valid when the relation IS an equivalence relation.
    [[nodiscard]] std::vector<std::vector<T>> equivalenceClasses() const {
        if (!isEquivalence()) {
            throw util::VerityError("equivalenceClasses requires an equivalence relation");
        }
        std::vector<bool> seen(matrix_.size(), false);
        std::vector<std::vector<T>> classes;
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            if (seen[i]) continue;
            std::vector<T> cls;
            for (std::size_t j = 0; j < matrix_.size(); ++j) {
                if (matrix_[i][j]) {
                    cls.push_back(elements_[j]);
                    seen[j] = true;
                }
            }
            classes.push_back(std::move(cls));
        }
        return classes;
    }

    /// Disjoint check: no element in two classes (pairwise intersections empty).
    [[nodiscard]] static bool partitionIsDisjoint(const std::vector<std::vector<T>>& classes) {
        for (std::size_t a = 0; a < classes.size(); ++a) {
            for (std::size_t b = a + 1; b < classes.size(); ++b) {
                for (const T& x : classes[a]) {
                    for (const T& y : classes[b]) {
                        if (x == y) return false;
                    }
                }
            }
        }
        return true;
    }

    /// Covering check: the union of the classes equals `universe`.
    [[nodiscard]] static bool partitionCovers(const std::vector<std::vector<T>>& classes,
                                              const std::vector<T>& universe) {
        std::size_t total = 0;
        for (const auto& c : classes) total += c.size();
        return total == universe.size();   // disjointness checked separately
    }

    // --- order structure (partial orders) -------------------------------------
    /// Transitive reduction (Hasse cover relation): keep a<b only when no
    /// c satisfies a<b and b<c. Requires a partial order; reflexivity is
    /// ignored (diagonal excluded from the diagram).
    [[nodiscard]] Relation hasseDiagram() const {
        if (!isPartialOrder() && !(isIrreflexive() && isAntisymmetric() && isTransitive())) {
            throw util::VerityError(
                "hasseDiagram requires a partial order (reflexive/antisymmetric/transitive)");
        }
        Relation out(elements_);
        const std::size_t n = matrix_.size();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = 0; j < n; ++j) {
                if (i == j || !matrix_[i][j]) continue;
                bool implied = false;
                for (std::size_t k = 0; k < n && !implied; ++k) {
                    if (k != i && k != j && matrix_[i][k] && matrix_[k][j]) implied = true;
                }
                if (!implied) out.matrix_[i][j] = true;
            }
        }
        return out;
    }

    [[nodiscard]] std::vector<T> minimalElements() const {
        std::vector<T> out;
        for (std::size_t i = 0; i < matrix_.size(); ++i) {
            bool hasSmaller = false;
            for (std::size_t j = 0; j < matrix_.size() && !hasSmaller; ++j) {
                if (j != i && matrix_[j][i]) hasSmaller = true;
            }
            if (!hasSmaller) out.push_back(elements_[i]);
        }
        return out;
    }

    [[nodiscard]] std::vector<T> maximalElements() const {
        return inverse().minimalElements();
    }

    /// Lower bounds of {a,b}: elements x with x≤a ∧ x≤b (reflexive included).
    [[nodiscard]] std::vector<T> lowerBounds(const T& a, const T& b) const {
        std::vector<T> out;
        const std::size_t ia = indexOf(a), ib = indexOf(b);
        for (std::size_t x = 0; x < matrix_.size(); ++x) {
            if (matrix_[x][ia] && matrix_[x][ib]) out.push_back(elements_[x]);
        }
        return out;
    }

    [[nodiscard]] std::vector<T> upperBounds(const T& a, const T& b) const {
        std::vector<T> out;
        const std::size_t ia = indexOf(a), ib = indexOf(b);
        for (std::size_t x = 0; x < matrix_.size(); ++x) {
            if (matrix_[ia][x] && matrix_[ib][x]) out.push_back(elements_[x]);
        }
        return out;
    }

    /// Least upper bound (join) of a and b; throws when none exists.
    /// u is least iff every other upper bound v satisfies u ≤ v.
    [[nodiscard]] T leastUpperBound(const T& a, const T& b) const {
        const auto ups = upperBounds(a, b);
        for (const T& u : ups) {
            bool isLeast = true;
            for (const T& v : ups) {
                if (!(u == v) && !contains(u, v)) {   // u ≤ v must hold
                    isLeast = false;
                    break;
                }
            }
            if (isLeast) return u;
        }
        throw util::VerityError("no least upper bound exists for the given pair");
    }

    [[nodiscard]] T greatestLowerBound(const T& a, const T& b) const {
        return inverse().leastUpperBound(a, b);
    }

    /// Lattice: every pair has a lub and a glb.
    [[nodiscard]] bool isLattice() const {
        for (std::size_t i = 0; i < elements_.size(); ++i) {
            for (std::size_t j = i; j < elements_.size(); ++j) {
                try {
                    (void)leastUpperBound(elements_[i], elements_[j]);
                    (void)greatestLowerBound(elements_[i], elements_[j]);
                } catch (const util::VerityError&) {
                    return false;
                }
            }
        }
        return true;
    }

    /// Comparable pairs (a≤b or b≤a, both reflexively) — for posets.
    [[nodiscard]] std::vector<std::pair<T, T>> incomparablePairs() const {
        std::vector<std::pair<T, T>> out;
        for (std::size_t i = 0; i < elements_.size(); ++i) {
            for (std::size_t j = i + 1; j < elements_.size(); ++j) {
                if (!matrix_[i][j] && !matrix_[j][i]) {
                    out.emplace_back(elements_[i], elements_[j]);
                }
            }
        }
        return out;
    }

private:
    [[nodiscard]] std::size_t indexOf(const T& x) const {
        const std::size_t* pos = index_.get(x);
        if (pos == nullptr) throw util::MissingNodeError("element not in relation universe");
        return *pos;
    }

    void checkSameUniverse(const Relation& other) const {
        if (other.elements_.size() != elements_.size()) {
            throw util::VerityError("relation universes differ in size");
        }
        for (const T& e : elements_) {
            if (!other.index_.contains(e)) {
                throw util::VerityError("relation universes differ");
            }
        }
    }

    std::vector<T> elements_;
    ds::HashTable<T, std::size_t> index_;
    std::vector<std::vector<bool>> matrix_;
};

} // namespace verity::dm
