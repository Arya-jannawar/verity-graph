#include "verity/dm/Logic.hpp"

#include <algorithm>
#include <cctype>
#include <stack>

namespace verity::dm {

namespace {

constexpr std::size_t kMaxVariables = 12;   // 2^12 = 4096 rows — the frozen guard

int precedence(Formula::Kind kind) {
    switch (kind) {
        case Formula::Kind::Not:     return 6;
        case Formula::Kind::And:     return 5;
        case Formula::Kind::Or:      return 4;
        case Formula::Kind::Xor:     return 3;
        case Formula::Kind::Implies: return 2;
        case Formula::Kind::Iff:     return 1;
        default:                     return 0;
    }
}

bool isRightAssociative(Formula::Kind kind) {
    return kind == Formula::Kind::Implies || kind == Formula::Kind::Iff;
}

Formula::Kind tokenKind(const std::string& token) {
    if (token == "&&" || token == "/\\" || token == "and") return Formula::Kind::And;
    if (token == "||" || token == "\\/" || token == "or") return Formula::Kind::Or;
    if (token == "=>" || token == "->") return Formula::Kind::Implies;
    if (token == "<=>" || token == "<->" || token == "==") return Formula::Kind::Iff;
    if (token == "!=" || token == "xor") return Formula::Kind::Xor;
    throw util::ParseError("unknown operator token: '" + token + "'");
}

bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

} // namespace

void Formula::collectVars(std::vector<std::string>& out) const {
    auto contains = [&out](const std::string& s) {
        return std::find(out.begin(), out.end(), s) != out.end();
    };
    switch (kind_) {
        case Kind::Var:
            if (!contains(name_)) out.push_back(name_);
            break;
        case Kind::Const:
            break;
        case Kind::Not:
            operand_->collectVars(out);
            break;
        default:
            left_->collectVars(out);
            right_->collectVars(out);
            break;
    }
}

FormulaPtr parseFormula(const std::string& text) {
    // tokenize
    std::vector<std::string> tokens;
    for (std::size_t i = 0; i < text.size();) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '(' || c == ')') {
            tokens.emplace_back(1, c);
            ++i;
            continue;
        }
        if (c == '!' || c == '~') {
            tokens.emplace_back("!");
            ++i;
            continue;
        }
        if (c == '&' || c == '|') {
            if (i + 1 < text.size() && text[i + 1] == c) {
                tokens.emplace_back(2, c);
                i += 2;
                continue;
            }
            throw util::ParseError("single '&' or '|' (use && / ||)");
        }
        if (c == '-' && i + 1 < text.size() && text[i + 1] == '>') {
            tokens.emplace_back("->");
            i += 2;
            continue;
        }
        if (c == '=') {
            if (text.compare(i, 2, "==") == 0) { tokens.emplace_back("=="); i += 2; continue; }
            if (text.compare(i, 2, "=>") == 0) { tokens.emplace_back("=>"); i += 2; continue; }
            throw util::ParseError("stray '=' in formula");
        }
        if (c == '<') {
            if (text.compare(i, 3, "<=>") == 0) { tokens.emplace_back("<=>"); i += 3; continue; }
            if (text.compare(i, 3, "<->") == 0) { tokens.emplace_back("<->"); i += 3; continue; }
            throw util::ParseError("stray '<' in formula");
        }
        if (isIdentChar(c)) {
            std::string word;
            while (i < text.size() && isIdentChar(text[i])) word.push_back(text[i++]);
            std::string lowerWord = word;
            std::transform(lowerWord.begin(), lowerWord.end(), lowerWord.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            if (lowerWord == "and" || lowerWord == "or" || lowerWord == "xor") {
                tokens.push_back(lowerWord);
            } else if (lowerWord == "not") {
                tokens.emplace_back("!");
            } else {
                tokens.push_back(std::move(word));   // variable or TRUE/FALSE
            }
            continue;
        }
        throw util::ParseError(std::string("unexpected character '") + c + "' in formula");
    }
    if (tokens.empty()) throw util::ParseError("empty formula");

    // shunting-yard to RPN, then build the tree
    std::stack<std::string> ops;
    std::vector<std::string> rpn;
    std::stack<std::size_t> arityStack;   // for unary Not handling
    for (const std::string& t : tokens) {
        if (t == "(") {
            ops.push(t);
        } else if (t == ")") {
            while (!ops.empty() && ops.top() != "(") {
                rpn.push_back(ops.top());
                ops.pop();
            }
            if (ops.empty()) throw util::ParseError("unbalanced parentheses");
            ops.pop();
        } else if (t == "!") {
            ops.push(t);
        } else if (t == "TRUE" || t == "FALSE" || t == "true" || t == "false" ||
                   (isIdentChar(t[0]) && t != "and" && t != "or" && t != "xor")) {
            rpn.push_back(t);
        } else {
            // binary operator
            const Formula::Kind kind = tokenKind(t);
            while (!ops.empty() && ops.top() != "(") {
                const std::string& top = ops.top();
                if (top == "!") {
                    rpn.push_back(top);
                    ops.pop();
                    continue;
                }
                const Formula::Kind topKind = tokenKind(top);
                if (precedence(topKind) > precedence(kind) ||
                    (precedence(topKind) == precedence(kind) && !isRightAssociative(kind))) {
                    rpn.push_back(top);
                    ops.pop();
                    continue;
                }
                break;
            }
            ops.push(t);
        }
    }
    while (!ops.empty()) {
        if (ops.top() == "(") throw util::ParseError("unbalanced parentheses");
        rpn.push_back(ops.top());
        ops.pop();
    }

    // build tree from RPN
    std::stack<FormulaPtr> build;
    for (const std::string& t : rpn) {
        if (t == "!") {
            if (build.empty()) throw util::ParseError("negation without operand");
            FormulaPtr inner = std::move(build.top());
            build.pop();
            build.push(Formula::makeNot(std::move(inner)));
            continue;
        }
        if (t == "TRUE" || t == "true") {
            build.push(Formula::makeConst(true));
            continue;
        }
        if (t == "FALSE" || t == "false") {
            build.push(Formula::makeConst(false));
            continue;
        }
        if (t == "and" || t == "or" || t == "xor" || t == "&&" || t == "||" ||
            t == "=>" || t == "->" || t == "<=>" || t == "<->" || t == "==" || t == "!=") {
            if (build.size() < 2) throw util::ParseError("operator '" + t + "' lacks operands");
            FormulaPtr r = std::move(build.top());
            build.pop();
            FormulaPtr l = std::move(build.top());
            build.pop();
            build.push(Formula::makeBinary(tokenKind(t), std::move(l), std::move(r)));
            continue;
        }
        build.push(Formula::makeVar(t));
    }
    if (build.size() != 1) throw util::ParseError("malformed formula (dangling operands)");
    FormulaPtr result = std::move(build.top());
    build.pop();
    return result;
}

namespace {

std::string kindSymbol(const Formula& f) {
    switch (f.kind()) {
        case Formula::Kind::Not:     return "!";
        case Formula::Kind::And:     return "&&";
        case Formula::Kind::Or:      return "||";
        case Formula::Kind::Implies: return "->";
        case Formula::Kind::Iff:     return "<->";
        case Formula::Kind::Xor:     return "xor";
        default:                     return "";
    }
}

} // namespace

std::string printPrefix(const Formula& f) {
    switch (f.kind()) {
        case Formula::Kind::Var:    return f.name();
        case Formula::Kind::Const:  return f.value() ? "TRUE" : "FALSE";
        case Formula::Kind::Not:    return "!(" + printPrefix(f.operand()) + ")";
        default:
            return "(" + kindSymbol(f) + " " + printPrefix(f.left()) + " " +
                   printPrefix(f.right()) + ")";
    }
}

std::string printInfix(const Formula& f) {
    switch (f.kind()) {
        case Formula::Kind::Var:    return f.name();
        case Formula::Kind::Const:  return f.value() ? "TRUE" : "FALSE";
        case Formula::Kind::Not:    return "!(" + printInfix(f.operand()) + ")";
        default:
            return "(" + printInfix(f.left()) + " " + kindSymbol(f) + " " +
                   printInfix(f.right()) + ")";
    }
}

std::string printPostfix(const Formula& f) {
    switch (f.kind()) {
        case Formula::Kind::Var:    return f.name();
        case Formula::Kind::Const:  return f.value() ? "TRUE" : "FALSE";
        case Formula::Kind::Not:    return printPostfix(f.operand()) + " !";
        default:
            return printPostfix(f.left()) + " " + printPostfix(f.right()) + " " +
                   kindSymbol(f);
    }
}

bool evaluate(const Formula& f, const std::map<std::string, bool>& assignment) {
    switch (f.kind()) {
        case Formula::Kind::Var: {
            const auto it = assignment.find(f.name());
            if (it == assignment.end()) {
                throw util::ParseError("unbound variable in evaluation: '" + f.name() + "'");
            }
            return it->second;
        }
        case Formula::Kind::Const:  return f.value();
        case Formula::Kind::Not:    return !evaluate(f.operand(), assignment);
        case Formula::Kind::And:    return evaluate(f.left(), assignment) && evaluate(f.right(), assignment);
        case Formula::Kind::Or:     return evaluate(f.left(), assignment) || evaluate(f.right(), assignment);
        case Formula::Kind::Implies: return !evaluate(f.left(), assignment) || evaluate(f.right(), assignment);
        case Formula::Kind::Iff:    return evaluate(f.left(), assignment) == evaluate(f.right(), assignment);
        case Formula::Kind::Xor:    return evaluate(f.left(), assignment) != evaluate(f.right(), assignment);
    }
    return false;
}

namespace {

std::vector<std::string> sortedVars(const Formula& f) {
    std::vector<std::string> vars;
    f.collectVars(vars);
    if (vars.size() > kMaxVariables) {
        throw util::ParseError("formula guard: >" + std::to_string(kMaxVariables) +
                               " distinct variables would exceed the bounded truth table");
    }
    std::sort(vars.begin(), vars.end());
    return vars;
}

} // namespace

std::vector<std::pair<std::map<std::string, bool>, bool>> truthTable(const Formula& f) {
    const std::vector<std::string> vars = sortedVars(f);
    std::vector<std::pair<std::map<std::string, bool>, bool>> rows;
    const std::size_t rowCount = std::size_t(1) << vars.size();
    rows.reserve(rowCount);
    for (std::size_t mask = 0; mask < rowCount; ++mask) {
        std::map<std::string, bool> assignment;
        for (std::size_t i = 0; i < vars.size(); ++i) {
            assignment[vars[i]] = (mask >> i) & 1u;
        }
        rows.emplace_back(assignment, evaluate(f, assignment));
    }
    return rows;
}

bool isTautology(const Formula& f) {
    for (const auto& [assignment, value] : truthTable(f)) {
        (void)assignment;
        if (!value) return false;
    }
    return true;
}

bool isContradiction(const Formula& f) {
    for (const auto& [assignment, value] : truthTable(f)) {
        (void)assignment;
        if (value) return false;
    }
    return true;
}

bool isSatisfiable(const Formula& f) {
    return !isContradiction(f);
}

bool semanticallyEquivalent(const Formula& a, const Formula& b) {
    std::vector<std::string> va;
    a.collectVars(va);
    std::vector<std::string> vb;
    b.collectVars(vb);
    std::vector<std::string> vars = va;
    for (const std::string& v : vb) {
        if (std::find(vars.begin(), vars.end(), v) == vars.end()) vars.push_back(v);
    }
    if (vars.size() > kMaxVariables) {
        throw util::ParseError("equivalence guard: combined variables exceed the bounded table");
    }
    std::sort(vars.begin(), vars.end());
    const std::size_t rowCount = std::size_t(1) << vars.size();
    for (std::size_t mask = 0; mask < rowCount; ++mask) {
        std::map<std::string, bool> assignment;
        for (std::size_t i = 0; i < vars.size(); ++i) {
            assignment[vars[i]] = (mask >> i) & 1u;
        }
        if (evaluate(a, assignment) != evaluate(b, assignment)) return false;
    }
    return true;
}

FormulaPtr toNNF(const Formula& f) {
    switch (f.kind()) {
        case Formula::Kind::Var: {
            auto out = Formula::makeVar(f.name());
            return out;
        }
        case Formula::Kind::Const:
            return Formula::makeConst(f.value());
        case Formula::Kind::Not: {
            const Formula& inner = f.operand();
            if (inner.kind() == Formula::Kind::Var) {
                return Formula::makeNot(Formula::makeVar(inner.name()));
            }
            if (inner.kind() == Formula::Kind::Const) {
                return Formula::makeConst(!inner.value());
            }
            if (inner.kind() == Formula::Kind::Not) {
                return toNNF(inner.operand());   // double negation
            }
            // De Morgan push-down
            if (inner.kind() == Formula::Kind::And) {
                return Formula::makeBinary(Formula::Kind::Or,
                                           Formula::makeNot(toNNF(inner.left())),
                                           Formula::makeNot(toNNF(inner.right())));
            }
            if (inner.kind() == Formula::Kind::Or) {
                return Formula::makeBinary(Formula::Kind::And,
                                           Formula::makeNot(toNNF(inner.left())),
                                           Formula::makeNot(toNNF(inner.right())));
            }
            // !(a -> b) == a && !b ; !(a <-> b) == a xor b ; !(a xor b) == a <-> b
            if (inner.kind() == Formula::Kind::Implies) {
                return Formula::makeBinary(Formula::Kind::And, toNNF(inner.left()),
                                           Formula::makeNot(toNNF(inner.right())));
            }
            if (inner.kind() == Formula::Kind::Iff) {
                return Formula::makeBinary(Formula::Kind::Xor, toNNF(inner.left()),
                                           toNNF(inner.right()));
            }
            return Formula::makeBinary(Formula::Kind::Iff, toNNF(inner.left()),
                                       toNNF(inner.right()));
        }
        case Formula::Kind::Implies:
            return Formula::makeBinary(Formula::Kind::Or, Formula::makeNot(toNNF(f.left())),
                                       toNNF(f.right()));
        case Formula::Kind::Iff:
            // a <-> b == (a && b) || (!a && !b)
            return Formula::makeBinary(
                Formula::Kind::Or,
                Formula::makeBinary(Formula::Kind::And, toNNF(f.left()), toNNF(f.right())),
                Formula::makeBinary(Formula::Kind::And, Formula::makeNot(toNNF(f.left())),
                                    Formula::makeNot(toNNF(f.right()))));
        case Formula::Kind::Xor:
            // a xor b == (a || b) && (!a || !b)
            return Formula::makeBinary(
                Formula::Kind::And,
                Formula::makeBinary(Formula::Kind::Or, toNNF(f.left()), toNNF(f.right())),
                Formula::makeBinary(Formula::Kind::Or, Formula::makeNot(toNNF(f.left())),
                                    Formula::makeNot(toNNF(f.right()))));
        default:
            return Formula::makeBinary(f.kind(), toNNF(f.left()), toNNF(f.right()));
    }
}

std::vector<std::vector<std::string>> toCNF(const Formula& f) {
    // Truth-table dual construction (bounded, always correct): one clause per
    // FALSIFYING assignment, containing the negation of that row's literals.
    std::vector<std::vector<std::string>> clauses;
    for (const auto& [assignment, value] : truthTable(f)) {
        if (value) continue;
        std::vector<std::string> clause;
        for (const auto& [name, v] : assignment) {
            clause.push_back(v ? "!" + name : name);
        }
        clauses.push_back(std::move(clause));
    }
    if (clauses.empty()) clauses.emplace_back(1, "TRUE");
    return clauses;
}

std::vector<std::vector<std::string>> toDNF(const Formula& f) {
    (void)sortedVars(f);   // guard
    // DNF via truth-table rows (bounded, deterministic, always correct):
    // one conjunction term per satisfying assignment.
    std::vector<std::vector<std::string>> terms;
    for (const auto& [assignment, value] : truthTable(f)) {
        if (!value) continue;
        std::vector<std::string> term;
        for (const auto& [name, v] : assignment) {
            term.push_back(v ? name : "!" + name);
        }
        terms.push_back(std::move(term));
    }
    if (terms.empty()) terms.emplace_back(1, "FALSE");
    return terms;
}

} // namespace verity::dm
