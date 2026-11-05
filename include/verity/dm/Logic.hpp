#pragma once
// VERITY GRAPH — dm/Logic: propositional logic engine (DM Unit 1, Phase 3).
//
// Formula expression tree (Var, Const, Not, And, Or, Implies, Iff, Xor),
// shunting-yard parser, prefix/infix/postfix printing, evaluation, truth
// tables, tautology/contradiction/satisfiability, equivalence checks
// (De Morgan, distributivity, contrapositive, absorption), CNF/DNF.
//
// EXPONENTIAL GUARDS: truth tables and CNF/DNF expansion are bounded by the
// variable count (<= 12 variables -> 4096 rows / 4096 terms max) and throw
// ParseError beyond, per the brief's bounded-combinatorics requirement.
//
// Complexity: parse O(n); eval O(n); truth table O(2^v · n); CNF/DNF
// O(2^v · size) worst case (guarded).

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

class Formula;
using FormulaPtr = std::unique_ptr<Formula>;

class Formula {
public:
    enum class Kind { Var, Const, Not, And, Or, Implies, Iff, Xor };

    explicit Formula(Kind kind, std::string name = "")
        : kind_(kind), name_(std::move(name)) {}

    Kind kind() const noexcept { return kind_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] bool value() const noexcept { return value_; }

    Formula& left() { return *left_; }
    [[nodiscard]] const Formula& left() const { return *left_; }
    Formula& right() { return *right_; }
    [[nodiscard]] const Formula& right() const { return *right_; }
    Formula& operand() { return *operand_; }
    [[nodiscard]] const Formula& operand() const { return *operand_; }

    void setLeft(FormulaPtr f) { left_ = std::move(f); }
    void setRight(FormulaPtr f) { right_ = std::move(f); }
    void setOperand(FormulaPtr f) { operand_ = std::move(f); }
    void setValue(bool v) noexcept { value_ = v; }

    [[nodiscard]] static FormulaPtr makeVar(std::string name) {
        return std::make_unique<Formula>(Kind::Var, std::move(name));
    }
    [[nodiscard]] static FormulaPtr makeConst(bool v) {
        auto f = std::make_unique<Formula>(Kind::Const);
        f->setValue(v);
        return f;
    }
    [[nodiscard]] static FormulaPtr makeNot(FormulaPtr inner) {
        auto f = std::make_unique<Formula>(Kind::Not);
        f->setOperand(std::move(inner));
        return f;
    }
    [[nodiscard]] static FormulaPtr makeBinary(Kind kind, FormulaPtr l, FormulaPtr r) {
        auto f = std::make_unique<Formula>(kind);
        f->setLeft(std::move(l));
        f->setRight(std::move(r));
        return f;
    }

    /// Collect variable names (deterministic: first-appearance order).
    void collectVars(std::vector<std::string>& out) const;

private:
    Kind kind_;
    std::string name_;   // Var only
    bool value_ = false;
    FormulaPtr left_, right_, operand_;
};

// --- parser (shunting-yard) -------------------------------------------------
//
// Grammar (loosest binding first): Iff | Xor > Implies > Or > And > Not > atoms
// Atoms: identifiers (letters/digits/underscore), TRUE/FALSE (case-insensitive),
// parentheses. Negation: ! or ~ or "not ".

[[nodiscard]] FormulaPtr parseFormula(const std::string& text);

// --- printing ---------------------------------------------------------------

[[nodiscard]] std::string printPrefix(const Formula& f);
[[nodiscard]] std::string printInfix(const Formula& f);
[[nodiscard]] std::string printPostfix(const Formula& f);

// --- evaluation and semantic properties --------------------------------------

/// Evaluate under `assignment` (variable -> bool). Throws on unbound var.
[[nodiscard]] bool evaluate(const Formula& f, const std::map<std::string, bool>& assignment);

/// All rows of the truth table: each row is (assignment, value). Guarded:
/// throws when the formula uses more than 12 distinct variables.
[[nodiscard]] std::vector<std::pair<std::map<std::string, bool>, bool>>
truthTable(const Formula& f);

[[nodiscard]] bool isTautology(const Formula& f);
[[nodiscard]] bool isContradiction(const Formula& f);
[[nodiscard]] bool isSatisfiable(const Formula& f);

/// Semantic equivalence under all assignments (guarded as truth tables).
[[nodiscard]] bool semanticallyEquivalent(const Formula& a, const Formula& b);

// --- normal forms --------------------------------------------------------------

/// Negation normal form (pushes negation to atoms; exponential only in
/// pathological cases, guarded via variable count).
[[nodiscard]] FormulaPtr toNNF(const Formula& f);

/// Conjunctive normal form: list of clauses, each clause a list of literals
/// ("P" or "!P"). Guarded: throws beyond 12 variables.
[[nodiscard]] std::vector<std::vector<std::string>> toCNF(const Formula& f);

/// Disjunctive normal form: list of terms, each term a list of literals.
[[nodiscard]] std::vector<std::vector<std::string>> toDNF(const Formula& f);

} // namespace verity::dm
