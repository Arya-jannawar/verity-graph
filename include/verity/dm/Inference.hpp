#pragma once
// VERITY GRAPH — dm/Inference: inference over knowledge bases (DM Unit 1).
//
// TWO DISTINCT formalisms (the brief explicitly requires keeping them honest):
//
// 1. DEFINITE HORN FORWARD CHAINING: facts are ground atoms ("p"), rules are
//    p1 && p2 -> head. Modus ponens is the engine. Every inferred fact
//    records the rule and its premises.
//
// 2. GENERAL-PURPOSE RULE DEMONSTRATIONS (modus tollens, disjunctive
//    syllogism, resolution) operate on EXPLICIT negation/clause
//    representations — they are NOT claimed to be ordinary definite-Horn
//    inference:
//      * modus tollens: rule a->b plus fact !b yields !a.
//      * disjunctive syllogism: a || b plus !a yields b.
//      * resolution: clause (l1 || l2) with clause (!l2 || l3) yields
//        (l1 || l3) — implemented over small clause sets.
//    Hypothetical syllogism chains Horn rules a->b, b->c into a->c.
//
// Complexity: forward chaining O(rounds * rules) with fixpoint detection
// (worst-case |facts| rounds); resolution over the small clause sets of the
// demos is O(n^3).

#include <string>
#include <vector>

#include "verity/util/VerityError.hpp"

namespace verity::dm {

struct HornRule {
    std::vector<std::string> premises;   // conjunctive body
    std::string head;                    // definite consequent
    std::string label;                   // rule id for derivations
};

struct DerivedFact {
    std::string atom;
    std::string rule;                    // rule label or "given"
    std::vector<std::string> premises;   // atoms used
};

/// Definite Horn forward chaining. Returns the full fact set (facts + all
/// inferred facts, in discovery order) with per-fact derivations.
[[nodiscard]] std::vector<DerivedFact> forwardChain(const std::vector<DerivedFact>& facts,
                                                    const std::vector<HornRule>& rules);

/// Hypothetical syllogism on Horn rules: a->b plus b->c yields a->c.
[[nodiscard]] std::vector<HornRule> hypotheticalSyllogism(const std::vector<HornRule>& rules);

/// Modus tollens over explicit literals: given a->b and the fact !b, derive !a.
/// Returns empty vector when !b is not among the facts.
[[nodiscard]] std::vector<DerivedFact> modusTollens(const HornRule& rule,
                                                    const std::vector<std::string>& knownFacts);

/// Disjunctive syllogism over explicit literals: (a || b) with !a yields b.
[[nodiscard]] std::string disjunctiveSyllogism(const std::vector<std::string>& disjunction,
                                               const std::string& negatedAtom);

/// Binary resolution: clause1 || clause2 over a literal that appears positively
/// in one and negatively in the other. Returns the resolvent (deduplicated);
/// throws when the clauses do not resolve.
struct Clause {
    std::vector<std::string> literals;   // "p" or "!p"
};

[[nodiscard]] Clause resolveBinary(const Clause& a, const Clause& b);

/// Full resolution over a small clause set until fixpoint or empty clause
/// (contradiction). Guarded: throws when more than 32 clauses are produced.
[[nodiscard]] std::vector<Clause> resolveAll(std::vector<Clause> clauses);

/// Literal helper: negation of "p" is "!p" and vice versa.
[[nodiscard]] inline std::string negateLiteral(const std::string& lit) {
    if (!lit.empty() && lit[0] == '!') return lit.substr(1);
    return "!" + lit;
}

[[nodiscard]] inline bool isNegated(const std::string& lit) {
    return !lit.empty() && lit[0] == '!';
}

} // namespace verity::dm
