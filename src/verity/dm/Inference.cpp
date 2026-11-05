#include "verity/dm/Inference.hpp"

#include <algorithm>
#include <set>

namespace verity::dm {

std::vector<DerivedFact> forwardChain(const std::vector<DerivedFact>& facts,
                                      const std::vector<HornRule>& rules) {
    std::vector<DerivedFact> derived(facts);
    std::set<std::string> known;
    for (const auto& f : derived) known.insert(f.atom);

    bool changed = true;
    while (changed) {
        changed = false;
        for (const HornRule& rule : rules) {
            if (known.count(rule.head) != 0u) continue;
            // all premises must be known facts
            bool ready = true;
            std::vector<std::string> usedPremises;
            for (const std::string& premise : rule.premises) {
                if (known.count(premise) == 0u) {
                    ready = false;
                    break;
                }
                usedPremises.push_back(premise);
            }
            if (!ready) continue;
            derived.push_back(DerivedFact{rule.head, rule.label, usedPremises});
            known.insert(rule.head);
            changed = true;
        }
    }
    return derived;
}

std::vector<HornRule> hypotheticalSyllogism(const std::vector<HornRule>& rules) {
    std::vector<HornRule> out = rules;
    bool changed = true;
    std::size_t guard = 0;
    while (changed && guard < 32) {
        changed = false;
        ++guard;
        const std::size_t original = out.size();
        for (std::size_t i = 0; i < original; ++i) {
            for (std::size_t j = 0; j < original; ++j) {
                if (i == j) continue;
                if (out[i].head == out[j].premises.front() && out[j].premises.size() == 1) {
                    HornRule chained{out[i].premises, out[j].head,
                                     out[i].label + "+" + out[j].label};
                    // deduplicate by (premises, head)
                    bool exists = false;
                    for (const HornRule& r : out) {
                        if (r.head == chained.head && r.premises == chained.premises) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        out.push_back(std::move(chained));
                        changed = true;
                    }
                }
            }
        }
    }
    return out;
}

std::vector<DerivedFact> modusTollens(const HornRule& rule,
                                      const std::vector<std::string>& knownFacts) {
    std::vector<DerivedFact> out;
    const std::string negatedHead = negateLiteral(rule.head);
    if (std::find(knownFacts.begin(), knownFacts.end(), negatedHead) == knownFacts.end()) {
        return out;   // no !head among the facts: nothing to derive
    }
    for (const std::string& premise : rule.premises) {
        // single-premise rules derive the negated premise directly; multi-
        // premise rules derive "at least one premise is false" as a disjunction
        if (rule.premises.size() == 1) {
            out.push_back(DerivedFact{negateLiteral(premise), "modus-tollens:" + rule.label,
                                      {negatedHead}});
        }
    }
    if (rule.premises.size() > 1) {
        // !p1 || !p2 || ... (explicit disjunction, NOT a Horn fact)
        out.push_back(DerivedFact{"!" + rule.premises.front() + "||!" + rule.premises.back(),
                                  "modus-tollens:" + rule.label, {negatedHead}});
    }
    return out;
}

std::string disjunctiveSyllogism(const std::vector<std::string>& disjunction,
                                 const std::string& negatedAtom) {
    const std::string target = negateLiteral(negatedAtom);   // !a -> a
    const bool hasNegation =
        std::find(disjunction.begin(), disjunction.end(), target) != disjunction.end();
    if (!hasNegation) {
        throw util::ParseError("disjunctive syllogism requires " + target +
                               " inside the disjunction");
    }
    for (const std::string& lit : disjunction) {
        if (lit != target) return lit;
    }
    throw util::ParseError("disjunction holds only the negated literal");
}

Clause resolveBinary(const Clause& a, const Clause& b) {
    for (const std::string& la : a.literals) {
        for (const std::string& lb : b.literals) {
            if (isNegated(la) != isNegated(lb) && negateLiteral(la) == lb) {
                Clause resolvent;
                for (const std::string& lit : a.literals) {
                    if (lit != la) resolvent.literals.push_back(lit);
                }
                for (const std::string& lit : b.literals) {
                    if (lit != lb) resolvent.literals.push_back(lit);
                }
                // deduplicate + sort for deterministic output
                std::sort(resolvent.literals.begin(), resolvent.literals.end());
                resolvent.literals.erase(
                    std::unique(resolvent.literals.begin(), resolvent.literals.end()),
                    resolvent.literals.end());
                return resolvent;
            }
        }
    }
    throw util::ParseError("clauses do not resolve (no complementary pair)");
}

std::vector<Clause> resolveAll(std::vector<Clause> clauses) {
    std::vector<Clause> all = std::move(clauses);
    bool changed = true;
    std::size_t guard = 0;
    while (changed && guard < 64) {
        ++guard;
        changed = false;
        const std::size_t n = all.size();
        for (std::size_t i = 0; i < n; ++i) {
            for (std::size_t j = i + 1; j < n; ++j) {
                Clause resolvent = resolveBinary(all[i], all[j]);
                if (resolvent.literals.empty()) {
                    // empty clause = contradiction reached; keep it and stop
                    all.push_back(std::move(resolvent));
                    return all;
                }
                bool exists = false;
                for (const Clause& c : all) {
                    if (c.literals == resolvent.literals) {
                        exists = true;
                        break;
                    }
                }
                if (!exists) {
                    if (all.size() >= 32) {
                        throw util::ParseError("resolution guard: clause set exceeded 32");
                    }
                    all.push_back(std::move(resolvent));
                    changed = true;
                }
            }
        }
    }
    return all;
}

} // namespace verity::dm
