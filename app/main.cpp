// VERITY GRAPH — `verity` command-line interface (Phase 2 slice).
//
// Commands:
//   help
//   ingest <extraction.json> [--json]
//   neighbors <id> [--depth k] [--json]
//   query evidence <name>
//   query autocomplete <prefix>
//   timeline
//   status
//   dm sets | dm numtheory | dm counting | dm relations | dm functions |
//      dm groups | dm combinatorics
//
// Cross-process convention (frozen): `ingest` validates and builds the graph
// in memory, then records the extraction path in out/verity_session.json.
// Every read command re-loads that extraction deterministically. The binary
// out/graph.vg persistence arrives in Phase 4.
//
// Exit codes: 0 success; 1 data/validation error; 2 usage error.
// Diagnostics go to stderr; results go to stdout.

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "verity/dm/Algebra.hpp"
#include "verity/dm/Combinatorics.hpp"
#include "verity/dm/Counting.hpp"
#include "verity/dm/FiniteSet.hpp"
#include "verity/dm/Functions.hpp"
#include "verity/dm/NumTheory.hpp"
#include "verity/dm/Relations.hpp"
#include "verity/ds/Trie.hpp"
#include "verity/engine/EntityResolver.hpp"
#include "verity/engine/TimelineBuilder.hpp"
#include "verity/engine/VerityEngine.hpp"
#include "verity/io/JsonLoader.hpp"
#include "verity/model/Node.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace {

constexpr int kExitOk = 0;
constexpr int kExitDataError = 1;
constexpr int kExitUsage = 2;

const char* kHelpText =
    "verity — document evidence graph (offline, fictional demo data)\n"
    "\n"
    "Usage:\n"
    "  verity help\n"
    "  verity ingest <extraction.json> [--json]\n"
    "  verity neighbors <id> [--depth k] [--json]\n"
    "  verity query evidence <name>\n"
    "  verity query autocomplete <prefix>\n"
    "  verity timeline\n"
    "  verity status\n"
    "  verity dm <sets|numtheory|counting|relations|functions|groups|combinatorics>\n"
    "\n"
    "Ingest records the extraction path in out/verity_session.json; read\n"
    "commands re-load it across processes. Exit codes: 0 ok, 1 error, 2 usage.\n";

struct Args {
    std::vector<std::string> positional;
    int depth = 1;
    bool json = false;
};

Args parseArgs(const std::vector<std::string>& tokens) {
    Args args;
    bool depthSeen = false;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const std::string& t = tokens[i];
        if (t == "--json") {
            args.json = true;
        } else if (t == "--depth") {
            if (i + 1 >= tokens.size()) {
                std::cerr << "usage: --depth requires a value\n";
                std::exit(kExitUsage);
            }
            args.depth = std::atoi(tokens[++i].c_str());
            depthSeen = true;
            if (args.depth < 0 || args.depth > 8) {
                std::cerr << "usage: --depth must be within [0,8]\n";
                std::exit(kExitUsage);
            }
        } else if (t.rfind("--", 0) == 0) {
            std::cerr << "usage: unknown option '" << t << "'\n";
            std::exit(kExitUsage);
        } else {
            args.positional.push_back(t);
        }
    }
    (void)depthSeen;
    return args;
}

std::string sessionPath() { return "out/verity_session.json"; }

void writeSession(const std::string& extractionPath) {
    nlohmann::json j;
    j["extraction"] = extractionPath;
    std::ofstream out(sessionPath());
    if (!out) {
        std::cerr << "warning: could not write " << sessionPath()
                  << " (create the out/ directory)\n";
        return;
    }
    out << j.dump(2) << "\n";
}

std::string readSessionExtraction() {
    std::ifstream in(sessionPath());
    if (!in) {
        throw verity::util::VerityError(
            "no session: run `verity ingest <extraction.json>` first (missing " +
            sessionPath() + ")");
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw verity::util::VerityError(std::string("corrupt session file: ") + e.what());
    }
    if (!j.contains("extraction") || !j.at("extraction").is_string()) {
        throw verity::util::VerityError("corrupt session file: missing 'extraction'");
    }
    return j.at("extraction").get<std::string>();
}

int cmdIngest(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity ingest <extraction.json>\n";
        return kExitUsage;
    }
    const std::string& path = args.positional[0];
    auto records = verity::io::JsonLoader::loadExtractionFile(path);
    verity::engine::VerityEngine engine;
    engine.ingest(records);
    writeSession(path);

    if (args.json) {
        nlohmann::json out;
        out["ok"] = true;
        out["documents"] = records.documents.size();
        out["entities"] = records.entities.size();
        out["claims"] = records.claims.size();
        out["events"] = records.events.size();
        out["citations"] = records.citations.size();
        out["graph_nodes"] = engine.graph().nodeCount();
        out["graph_edges"] = engine.graph().edgeCount();
        std::cout << out.dump(2) << "\n";
    } else {
        std::cout << "ingested " << path << ": "
                  << records.documents.size() << " documents, "
                  << records.entities.size() << " entities, "
                  << records.claims.size() << " claims, "
                  << records.events.size() << " events, "
                  << records.citations.size() << " citations\n";
        std::cout << "graph: " << engine.graph().nodeCount() << " nodes, "
                  << engine.graph().edgeCount() << " edges\n";
        std::cout << "session saved to " << sessionPath() << "\n";
    }
    return kExitOk;
}

int cmdNeighbors(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity neighbors <id> [--depth k]\n";
        return kExitUsage;
    }
    const verity::model::NodeId id(args.positional[0]);
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto hood = engine.neighbors(id, args.depth);
    if (hood.empty()) {
        std::cerr << "error: unknown id '" << id.str() << "'\n";
        return kExitDataError;
    }
    if (args.json) {
        nlohmann::json out;
        out["ok"] = true;
        out["id"] = id.str();
        out["depth"] = args.depth;
        out["nodes"] = nlohmann::json::array();
        for (const auto& [nid, dist] : hood) {
            out["nodes"].push_back({{"id", nid.str()}, {"distance", dist}});
        }
        std::cout << out.dump(2) << "\n";
    } else {
        std::cout << "BFS neighborhood of " << id.str() << " (depth " << args.depth
                  << "): " << hood.size() << " nodes\n";
        for (const auto& [nid, dist] : hood) {
            const auto* node = engine.store().find(nid);
            std::cout << "  " << dist << "  " << nid.str() << "  "
                      << (node ? node->type() : "?") << "  "
                      << (node ? node->label() : "") << "\n";
        }
    }
    return kExitOk;
}

int cmdQueryEvidence(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "usage: verity query evidence <name>\n";
        return kExitUsage;
    }
    std::string name = args.positional[0];
    for (std::size_t i = 1; i < args.positional.size(); ++i) name += " " + args.positional[i];
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto hits = engine.queryEvidence(name);
    if (hits.empty()) {
        std::cout << "no evidence found for '" << name << "'\n";
        return kExitOk;
    }
    std::cout << "evidence for '" << name << "': " << hits.size() << " claim(s)\n";
    for (const auto& hit : hits) {
        std::cout << "  " << hit.claim.str() << "  " << hit.predicate << " "
                  << hit.objectText << "  [" << hit.entity.str() << " @"
                  << hit.document.str() << ", confidence " << hit.confidence << "]\n"
                  << "    quote: \"" << hit.quote << "\"\n";
    }
    return kExitOk;
}

int cmdStatus() {
    const std::string path = readSessionExtraction();
    std::cout << "session: " << sessionPath() << "\n"
              << "extraction: " << path << "\n";
    return kExitOk;
}

int cmdDm(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity dm <sets|numtheory|counting|relations|functions|groups|combinatorics>\n";
        return kExitUsage;
    }
    const std::string& which = args.positional[0];
    if (which == "sets") {
        using verity::dm::FiniteSet;
        const FiniteSet<int> a = FiniteSet<int>::fromVector({1, 2, 3, 4});
        const FiniteSet<int> b = FiniteSet<int>::fromVector({3, 4, 5});
        std::cout << "A = {1,2,3,4}  B = {3,4,5}\n";
        std::cout << "A ∪ B = " << a.unionWith(b).size() << " elements {";
        bool first = true;
        for (const int v : a.unionWith(b).toVector()) {
            std::cout << (first ? "" : ",") << v;
            first = false;
        }
        std::cout << "}\n";
        std::cout << "A ∩ B = " << a.intersectWith(b).size() << " elements\n";
        std::cout << "inclusion-exclusion |A∪B| = " << verity::dm::unionSizeInclusionExclusion(a, b)
                  << " (brute force " << verity::dm::unionSizeBruteForce(a, b) << ")\n";
        std::cout << "|P(A)| = " << a.powerSet().size() << " (2^4)\n";
        return kExitOk;
    }
    if (which == "numtheory") {
        std::cout << "gcd(240,46) = " << verity::dm::gcd(240, 46) << "\n";
        std::cout << "modpow(2,10,1000) = " << verity::dm::modPow(2, 10, 1000) << "\n";
        std::cout << "modinv(3,11) = " << verity::dm::modInverse(3, 11) << "\n";
        std::cout << "isprime(97) = " << (verity::dm::isPrime(97) ? "true" : "false") << "\n";
        std::cout << "nextprime(64) = " << verity::dm::nextPrime(64) << "\n";
        std::cout << "phi(12) = " << verity::dm::eulerTotient(12) << "\n";
        return kExitOk;
    }
    if (which == "counting") {
        std::cout << "20! = " << verity::dm::factorial(20) << "\n";
        std::cout << "C(52,5) = " << verity::dm::nCr(52, 5) << "\n";
        std::cout << "P(10,3) = " << verity::dm::nPr(10, 3) << "\n";
        std::cout << "pascal C(18,9) = " << verity::dm::pascalNcr(18, 9)
                  << " == formula " << verity::dm::nCr(18, 9) << "\n";
        std::cout << "binomial theorem check (2,3,7): "
                  << (verity::dm::binomialTheoremCheck(2, 3, 7) ? "holds" : "FAILS") << "\n";
        return kExitOk;
    }
    std::cerr << "usage: verity dm <sets|numtheory|counting>\n";
    return kExitUsage;
}

int cmdTimeline() {
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto result = verity::engine::TimelineBuilder::build(engine.records());
    if (result.finding.has_value()) {
        const auto& f = *result.finding;
        std::cout << f.statement << "\n";
        std::cout << "witness path:";
        for (const auto& id : f.witness) std::cout << " " << id;
        std::cout << "\nclaims on the cycle:";
        for (const auto& id : f.claimIds) std::cout << " " << id;
        std::cout << "\nHasse diagram and topological order withheld for cyclic data\n";
        return kExitOk;
    }
    std::cout << "chronological order:";
    for (const auto& id : result.chronologicalOrder) std::cout << " " << id;
    std::cout << "\ntopological order:";
    for (const auto& id : result.topologicalOrder) std::cout << " " << id;
    std::cout << "\nHasse edges:";
    for (const auto& [a, b] : result.hasseEdges) std::cout << " " << a << "->" << b;
    std::cout << "\nincomparable pairs: " << result.incomparablePairs.size() << "\n";
    std::cout << "DOT:\n" << result.hasseDot;
    return kExitOk;
}

int cmdAutocomplete(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "usage: verity query autocomplete <prefix>\n";
        return kExitUsage;
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    verity::ds::Trie trie;
    for (const auto& entity : engine.records().entities) {
        trie.insert(entity.name);
        for (const std::string& alias : entity.aliases) trie.insert(alias);
    }
    const auto words = trie.wordsWithPrefix(args.positional[0]);
    if (words.empty()) {
        std::cout << "no matches for prefix '" << args.positional[0] << "'\n";
        return kExitOk;
    }
    std::cout << "autocomplete '" << args.positional[0] << "': " << words.size() << " match(es)\n";
    for (const auto& w : words) std::cout << "  " << w << "\n";
    return kExitOk;
}

int cmdDmRelations() {
    // divisibility on divisors of 12: reflexive antisymmetric transitive
    const std::vector<int> elems{1, 2, 3, 4, 6, 12};
    verity::dm::Relation<int> r(elems);
    for (const int a : elems) {
        for (const int b : elems) {
            if (b % a == 0) r.add(a, b);
        }
    }
    std::cout << "R = divides on {1,2,3,4,6,12}\n";
    std::cout << "reflexive=" << r.isReflexive() << " antisymmetric=" << r.isAntisymmetric()
              << " transitive=" << r.isTransitive() << " partialOrder=" << r.isPartialOrder()
              << " lattice=" << r.isLattice() << "\n";
    const auto hasse = r.hasseDiagram();
    std::cout << "Hasse edges:";
    for (const auto& a : hasse.elements()) {
        for (const auto& b : hasse.elements()) {
            if (a < b && hasse.contains(a, b)) std::cout << " " << a << "<" << b;
        }
    }
    std::cout << "\nlub(2,3) = " << r.leastUpperBound(2, 3)
              << "  glb(4,6) = " << r.greatestLowerBound(4, 6) << "\n";
    // equivalence closure demo
    verity::dm::Relation<int> s(std::vector<int>{1, 2, 3});
    s.add(1, 2);
    const auto cls = s.equivalenceClosure().equivalenceClasses();
    std::cout << "closure of {1~2} over {1,2,3}: " << cls.size() << " classes\n";
    return kExitOk;
}

int cmdDmFunctions() {
    const verity::dm::FiniteSet<int> domain = verity::dm::FiniteSet<int>::fromVector({1, 2, 3});
    const verity::dm::FiniteSet<int> codomain = verity::dm::FiniteSet<int>::fromVector({4, 5, 6});
    verity::dm::FiniteFunction<int, int> f(domain, codomain, {{1, 4}, {2, 5}, {3, 6}});
    std::cout << "f: {1,2,3}->{4,5,6}, f(x)=x+3: total=" << f.isTotal()
              << " injective=" << f.isInjective() << " surjective=" << f.isSurjective()
              << " bijective=" << f.isBijective() << "\n";
    const auto inv = f.inverse();
    std::cout << "f^-1(5) = " << *inv.apply(5) << "\n";
    // composition with itself is defined on codomain==domain? not here; use square domain
    const verity::dm::FiniteSet<int> sq = verity::dm::FiniteSet<int>::fromVector({0, 1, 2});
    verity::dm::FiniteFunction<int, int> g(sq, sq, {{0, 1}, {1, 2}, {2, 0}});
    const auto g2 = verity::dm::FiniteFunction<int, int>::compose(g, g);
    std::cout << "g^2(0) = " << *g2.apply(0) << "\n";
    return kExitOk;
}

int cmdDmGroups() {
    const verity::dm::ZnPlus z7{7};
    const auto elems = z7.elements();
    const auto addMod7 = [&z7](std::uint64_t a, std::uint64_t b) { return z7.op(a, b); };
    std::cout << "(Z_7,+): group=" << verity::dm::groupHolds(elems, addMod7, z7.identity())
              << " generators:";
    for (const auto g : z7.cyclicGenerators()) std::cout << " " << g;
    std::cout << "\n";
    const auto sub = z7.subgroupGeneratedBy(2);
    std::cout << "<2> = {";
    for (std::size_t i = 0; i < sub.size(); ++i) std::cout << (i ? "," : "") << sub[i];
    std::cout << "} order " << sub.size() << " divides 7: "
              << verity::dm::lagrangeHolds(elems.size(), sub.size()) << "\n";
    const verity::dm::ZpMul zp{11};
    const auto mulMod11 = [&zp](std::uint64_t a, std::uint64_t b) { return zp.op(a, b); };
    std::cout << "(Z_11*,x): group="
              << verity::dm::groupHolds(zp.elements(), mulMod11, zp.identity())
              << " generators: " << zp.cyclicGenerators().size() << "\n";
    // honest failing example: (Z_6, +) is a group, but (Z_6, ×) is NOT
    // (0 has no inverse; only units {1,5} would form one)
    const std::vector<int> z6{0, 1, 2, 3, 4, 5};
    const auto add6 = [](int a, int b) { return (a + b) % 6; };
    const auto mul6 = [](int a, int b) { return (a * b) % 6; };
    std::cout << "(Z_6,+) group=" << verity::dm::groupHolds(z6, add6, 0) << "\n";
    std::cout << "(Z_6, x) group (honest failure)="
              << verity::dm::groupHolds(z6, mul6, 1) << "\n";
    return kExitOk;
}

int cmdDmCombinatorics() {
    std::vector<std::string> seq{"a", "c", "b"};
    std::vector<std::string> state = seq;
    int count = 1;
    while (verity::dm::nextPermutation(state)) ++count;
    std::cout << "permutations of {a,c,b} enumerated: " << count << " (3! = 6)\n";
    const std::vector<std::vector<bool>> poset{
        {false, true, false, false},
        {false, false, true, false},
        {false, false, false, true},
        {false, false, false, false}};
    std::cout << "linear extensions of chain a<b<c<d: "
              << verity::dm::countLinearExtensions(poset) << " (expected 1)\n";
    const std::vector<std::vector<bool>> diamond{
        {false, true, true, false},
        {false, false, false, true},
        {false, false, false, true},
        {false, false, false, false}};
    std::cout << "linear extensions of diamond: "
              << verity::dm::countLinearExtensions(diamond) << " (expected 2)\n";
    std::cout << "pigeonhole (10 items, 9 boxes): "
              << (verity::dm::pigeonholeHolds(10, 9) ? "some box has >=2" : "no conclusion") << "\n";
    return kExitOk;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> tokens(argv + 1, argv + argc);
    if (tokens.empty()) {
        std::cerr << kHelpText;
        return kExitUsage;
    }
    const std::string command = tokens[0];
    const Args args = parseArgs(std::vector<std::string>(tokens.begin() + 1, tokens.end()));

    try {
        if (command == "help" || command == "--help" || command == "-h") {
            std::cout << kHelpText;
            return kExitOk;
        }
        if (command == "ingest") return cmdIngest(args);
        if (command == "neighbors") return cmdNeighbors(args);
        if (command == "query") {
            if (args.positional.empty()) {
                std::cerr << "usage: verity query evidence <name> | query autocomplete <prefix>\n";
                return kExitUsage;
            }
            if (args.positional[0] == "evidence") {
                auto sub = args;
                sub.positional.erase(sub.positional.begin());
                return cmdQueryEvidence(sub);
            }
            if (args.positional[0] == "autocomplete") {
                auto sub = args;
                sub.positional.erase(sub.positional.begin());
                return cmdAutocomplete(sub);
            }
            std::cerr << "usage: verity query evidence <name> | query autocomplete <prefix>\n";
            return kExitUsage;
        }
        if (command == "timeline") return cmdTimeline();
        if (command == "status") return cmdStatus();
        if (command == "dm") {
            if (!args.positional.empty() && args.positional[0] == "relations") return cmdDmRelations();
            if (!args.positional.empty() && args.positional[0] == "functions") return cmdDmFunctions();
            if (!args.positional.empty() && args.positional[0] == "groups") return cmdDmGroups();
            if (!args.positional.empty() && args.positional[0] == "combinatorics") {
                return cmdDmCombinatorics();
            }
            return cmdDm(args);
        }
        std::cerr << "usage: unknown command '" << command << "'\n" << kHelpText;
        return kExitUsage;
    } catch (const verity::util::VerityError& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kExitDataError;
    } catch (const std::exception& e) {
        std::cerr << "internal error: " << e.what() << "\n";
        return kExitDataError;
    }
}
