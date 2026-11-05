// VERITY GRAPH — tests/dm/test_logic.cpp
// Logic engine: parser, printing, evaluation, truth tables, tautology/
// contradiction/satisfiability, equivalences, CNF/DNF, guards, inference
// (Horn forward chaining + explicit-literal rules), quantifiers.

#include "doctest/doctest.h"

#include <map>
#include <stdexcept>
#include <vector>

#include "verity/dm/Inference.hpp"
#include "verity/dm/Logic.hpp"
#include "verity/dm/Quantifiers.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::dm;

TEST_CASE("parser: builds the right AST shape") {
    const auto f = parseFormula("p && q -> r");
    CHECK(f->kind() == Formula::Kind::Implies);
    CHECK(f->left().kind() == Formula::Kind::And);
    CHECK(f->left().left().kind() == Formula::Kind::Var);
    CHECK(f->left().left().name() == "p");
    CHECK(f->right().name() == "r");

    const auto g = parseFormula("!(p || q)");
    CHECK(g->kind() == Formula::Kind::Not);
    CHECK(g->operand().kind() == Formula::Kind::Or);

    const auto c = parseFormula("TRUE && FALSE");
    CHECK(c->left().kind() == Formula::Kind::Const);
    CHECK(c->left().value() == true);
}

TEST_CASE("parser: precedence and associativity") {
    // a || b -> c parses as (a || b) -> c (Or binds tighter than Implies)
    const auto f = parseFormula("a || b -> c");
    CHECK(f->kind() == Formula::Kind::Implies);
    CHECK(f->left().kind() == Formula::Kind::Or);
    // a -> b -> c parses right-associatively: a -> (b -> c)
    const auto g = parseFormula("a -> b -> c");
    CHECK(g->kind() == Formula::Kind::Implies);
    CHECK(g->right().kind() == Formula::Kind::Implies);
    // a && b && c groups left: ((a && b) && c)
    const auto h = parseFormula("a && b && c");
    CHECK(h->kind() == Formula::Kind::And);
    CHECK(h->left().kind() == Formula::Kind::And);
}

TEST_CASE("parser: failure paths") {
    CHECK_THROWS_AS((void)parseFormula(""), verity::util::ParseError);
    CHECK_THROWS_AS((void)parseFormula("p &&"), verity::util::ParseError);
    CHECK_THROWS_AS((void)parseFormula("(p"), verity::util::ParseError);
    CHECK_THROWS_AS((void)parseFormula("p)"), verity::util::ParseError);
    CHECK_THROWS_AS((void)parseFormula("p & q"), verity::util::ParseError);
    CHECK_THROWS_AS((void)parseFormula("&& p"), verity::util::ParseError);
}

TEST_CASE("printing: prefix, infix, postfix") {
    const auto f = parseFormula("p && q -> r");
    CHECK_EQ(printInfix(*f), std::string("((p && q) -> r)"));
    CHECK_EQ(printPrefix(*f), std::string("(-> (&& p q) r)"));
    CHECK_EQ(printPostfix(*f), std::string("p q && r ->"));
}

TEST_CASE("evaluation and truth tables") {
    const auto f = parseFormula("p && q");
    std::map<std::string, bool> env{{"p", true}, {"q", false}};
    CHECK_FALSE(evaluate(*f, env));
    env["q"] = true;
    CHECK(evaluate(*f, env));
    CHECK_THROWS_AS((void)evaluate(*f, std::map<std::string, bool>{{"p", true}}),
                    verity::util::ParseError);

    const auto iff = parseFormula("p <-> q");
    const auto table = truthTable(*iff);
    CHECK_EQ(table.size(), 4u);   // 2 variables
    // rows in sorted-variable order: (F,F),(F,T),(T,F),(T,T)
    CHECK(table[0].second);      // F,F -> iff true
    CHECK_FALSE(table[1].second);
    CHECK_FALSE(table[2].second);
    CHECK(table[3].second);
}

TEST_CASE("tautology, contradiction, satisfiability") {
    CHECK(isTautology(*parseFormula("p -> p")));
    CHECK(isTautology(*parseFormula("!(p && q) <-> (!p || !q)")));   // De Morgan
    CHECK(isContradiction(*parseFormula("p && !p")));
    CHECK(isSatisfiable(*parseFormula("p && q")));
    CHECK_FALSE(isSatisfiable(*parseFormula("p && !p")));
    CHECK_FALSE(isTautology(*parseFormula("p && q")));
}

TEST_CASE("semantic equivalence: De Morgan, distributivity, contrapositive, absorption") {
    CHECK(semanticallyEquivalent(*parseFormula("!(p && q)"), *parseFormula("!p || !q")));
    CHECK(semanticallyEquivalent(*parseFormula("!(p || q)"), *parseFormula("!p && !q")));
    CHECK(semanticallyEquivalent(*parseFormula("p && (q || r)"),
                                 *parseFormula("(p && q) || (p && r)")));
    CHECK(semanticallyEquivalent(*parseFormula("p -> q"), *parseFormula("!q -> !p")));
    CHECK(semanticallyEquivalent(*parseFormula("p && (p || q)"), *parseFormula("p")));
    CHECK_FALSE(semanticallyEquivalent(*parseFormula("p -> q"), *parseFormula("q -> p")));
}

TEST_CASE("CNF/DNF: bounded construction") {
    // p -> q is FALSE only when p=T,q=F: CNF = (!p || q)
    const auto impl = parseFormula("p -> q");
    const auto cnf = toCNF(*impl);
    REQUIRE_EQ(cnf.size(), 1u);
    const std::vector<std::string> expected{"!p", "q"};
    CHECK_EQ(cnf[0], expected);

    // DNF of p && q: exactly one satisfying term
    const auto conj = parseFormula("p && q");
    const auto dnf = toDNF(*conj);
    REQUIRE_EQ(dnf.size(), 1u);
    const std::vector<std::string> term{"p", "q"};
    CHECK_EQ(dnf[0], term);

    // contradiction: empty DNF -> the FALSE constant
    const auto contra = parseFormula("p && !p");
    const auto dnf2 = toDNF(*contra);
    CHECK_EQ(dnf2.size(), 1u);
    CHECK_EQ(dnf2[0][0], "FALSE");

    // tautology: empty CNF -> the TRUE constant
    const auto taut = parseFormula("p || !p");
    const auto cnf2 = toCNF(*taut);
    CHECK_EQ(cnf2.size(), 1u);
    CHECK_EQ(cnf2[0][0], "TRUE");

    // guard: more than 12 variables must throw rather than explode
    std::string big = "v0";
    for (int i = 1; i <= 12; ++i) big += " && v" + std::to_string(i);
    CHECK_THROWS_AS((void)toCNF(*parseFormula(big)), verity::util::ParseError);
}

TEST_CASE("inference: definite Horn forward chaining with derivations") {
    const std::vector<DerivedFact> facts{
        {"human", "given", {}},
        {"mortal_rule_human", "given", {}}};
    const std::vector<HornRule> rules{
        {{"human"}, "mortal", "R1"},
        {{"mortal"}, "legacy", "R2"},
        {{"human", "thinker"}, "philosopher", "R3"}};
    const auto derived = forwardChain(facts, rules);

    bool hasMortal = false, hasLegacy = false, hasPhilosopher = false;
    for (const auto& fact : derived) {
        if (fact.atom == "mortal") {
            hasMortal = true;
            CHECK(fact.rule == "R1");
            CHECK(fact.premises == std::vector<std::string>{"human"});
        }
        hasLegacy = hasLegacy || fact.atom == "legacy";
        hasPhilosopher = hasPhilosopher || fact.atom == "philosopher";
    }
    CHECK(hasMortal);
    CHECK(hasLegacy);        // chained via R2
    CHECK_FALSE(hasPhilosopher);   // premise "thinker" unknown
}

TEST_CASE("inference: hypothetical syllogism chains Horn rules") {
    const std::vector<HornRule> rules{
        {{"a"}, "b", "S1"},
        {{"b"}, "c", "S2"}};
    const auto chained = hypotheticalSyllogism(rules);
    bool found = false;
    for (const auto& rule : chained) {
        if (rule.head == "c" && rule.premises == std::vector<std::string>{"a"}) {
            found = true;
            CHECK(rule.label.find("S1") != std::string::npos);
        }
    }
    CHECK(found);
}

TEST_CASE("inference: modus tollens and disjunctive syllogism over explicit literals") {
    const HornRule rule{{"p"}, "q", "MT1"};
    const auto derived = modusTollens(rule, {"!q"});
    REQUIRE_EQ(derived.size(), 1u);
    CHECK_EQ(derived[0].atom, "!p");
    CHECK(modusTollens(rule, {"q"}).empty());   // no !q: nothing to derive

    // multi-premise rules derive an explicit disjunction, not a Horn fact
    const HornRule multi{{"p", "r"}, "q", "MT2"};
    const auto out = modusTollens(multi, {"!q"});
    REQUIRE_EQ(out.size(), 1u);
    CHECK(out[0].atom == "!p||!r");

    CHECK_EQ(disjunctiveSyllogism({"a", "b"}, "!a"), std::string("b"));
    CHECK_EQ(disjunctiveSyllogism({"a", "b"}, "!b"), std::string("a"));
    CHECK_THROWS_AS((void)disjunctiveSyllogism({"a", "b"}, "!c"), verity::util::ParseError);
}

TEST_CASE("inference: binary resolution and contradiction detection") {
    const Clause a{{"p", "q"}};
    const Clause b{{"!q", "r"}};
    const auto resolvent = resolveBinary(a, b);
    const std::vector<std::string> expected{"p", "r"};
    CHECK_EQ(resolvent.literals, expected);
    CHECK_THROWS_AS((void)resolveBinary(Clause{{"p"}}, Clause{{"q"}}), verity::util::ParseError);

    // p and !p resolve to the EMPTY clause (contradiction found)
    const auto all = resolveAll(std::vector<Clause>{{{"p"}}, {{"!p"}}, {{"r"}}});
    CHECK_EQ(all.back().literals.size(), 0u);
}

TEST_CASE("quantifiers: finite domains and negation laws") {
    const std::vector<int> domain{2, 4, 6, 8};
    const auto isEven = [](int x) { return x % 2 == 0; };
    CHECK(forAll(domain, isEven));
    CHECK(exists(domain, [](int x) { return x == 6; }));
    CHECK_FALSE(exists(domain, [](int x) { return x == 5; }));
    CHECK(existsUnique(domain, [](int x) { return x == 6; }));
    CHECK_FALSE(existsUnique(domain, isEven));

    // quantifier negation verified over the domain:
    // !(forall p) == exists !p  and  !(exists p) == forall !p
    const auto isOdd = [](int x) { return x % 2 != 0; };
    const std::vector<int> mixed{2, 3, 4};
    CHECK(!forAll(mixed, isEven) == exists(mixed, isOdd));
    const auto overTen = [](int x) { return x > 10; };
    const auto notOverTen = [](int x) { return x <= 10; };
    CHECK(!exists(mixed, overTen) == forAll(mixed, notOverTen));

    // nested quantifiers: every person has an age >= 0
    const auto ageNonNegative = [](int age) { return age >= 0; };
    const std::vector<std::pair<std::string, int>> people{{"a", 1}, {"b", 5}};
    CHECK(forAll(people, [&ageNonNegative](const std::pair<std::string, int>& p) {
        return forAll(std::vector<int>{p.second}, ageNonNegative);
    }));
}
