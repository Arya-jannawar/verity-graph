#!/usr/bin/env python
"""One-shot development patch: wires the Phase-3 CLI commands into app/main.cpp.
Runs once; kept for provenance."""
import io

p = "app/main.cpp"
src = open(p, encoding="utf-8").read()

src = src.replace(
    '#include "verity/ds/Trie.hpp"\n#include "verity/engine/EntityResolver.hpp"',
    '#include "verity/ds/GraphAlgorithms.hpp"\n'
    '#include "verity/ds/StringSearch.hpp"\n'
    '#include "verity/ds/Trie.hpp"\n'
    '#include "verity/dm/GraphAnalysis.hpp"\n'
    '#include "verity/dm/Inference.hpp"\n'
    '#include "verity/dm/Logic.hpp"\n'
    '#include "verity/dm/NoisyOr.hpp"\n'
    '#include "verity/dm/Quantifiers.hpp"\n'
    '#include "verity/engine/ConflictDetector.hpp"\n'
    '#include "verity/engine/EntityResolver.hpp"')
src = src.replace(
    '#include "verity/engine/TimelineBuilder.hpp"\n#include "verity/engine/VerityEngine.hpp"',
    '#include "verity/engine/Reasoning.hpp"\n'
    '#include "verity/engine/TimelineBuilder.hpp"\n'
    '#include "verity/engine/VerityEngine.hpp"')

src = src.replace(
    '    "  verity timeline\\n"\n    "  verity status\\n"',
    '    "  verity timeline\\n"\n'
    '    "  verity conflicts [--rules data/rules/rules.txt]\\n"\n'
    '    "  verity circular\\n"\n'
    '    "  verity keyentities\\n"\n'
    '    "  verity backbone\\n"\n'
    '    "  verity path <from-id> <to-id>\\n"\n'
    '    "  verity top-evidence <claim-id>\\n"\n'
    '    "  verity trace <claim-id>\\n"\n'
    '    "  verity status\\n"')
src = src.replace(
    '"  verity dm <sets|numtheory|counting|relations|functions|groups|combinatorics>\\n"',
    '"  verity dm <sets|numtheory|counting|relations|functions|groups|combinatorics|logic|graphs>\\n"')

marker = "} // namespace\n\nint main(int argc, char** argv) {"
new_cmds = r'''
std::vector<Conflict> runConflictDetection(const std::string& rulesPath,
                                           verity::engine::ConflictJsonCollector& collector) {
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto resolution = verity::engine::EntityResolver::resolve(engine.records());
    const auto timeline = verity::engine::TimelineBuilder::build(engine.records());
    const auto context = verity::engine::ConflictDetector::buildContext(
        engine.records(), resolution, timeline);

    std::ifstream rulesFile(rulesPath);
    if (!rulesFile) {
        throw verity::util::VerityError("cannot open rules file: '" + rulesPath +
                                        "' (run from the repo root or pass --rules)");
    }
    verity::engine::ConflictDetector detector;
    std::string line;
    while (std::getline(rulesFile, line)) {
        auto rule = verity::engine::ConflictDetector::RuleFactory::fromText(line);
        if (rule) detector.addRule(std::move(rule));
    }
    detector.addListener(&collector);
    return detector.run(context);
}

int cmdConflicts(const Args& args) {
    std::string rulesPath = "data/rules/rules.txt";
    for (std::size_t i = 0; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--rules") rulesPath = args.positional[i + 1];
    }
    verity::engine::ConflictJsonCollector collector;
    const auto findings = runConflictDetection(rulesPath, collector);
    std::cout << findings.size() << " finding(s)\n";
    for (const auto& finding : findings) {
        std::cout << finding.id << " [" << finding.severity << "] " << finding.rule << ": "
                  << finding.statement << "\n";
        std::cout << "  subject " << finding.subjectId << " predicate " << finding.predicate
                  << " claims " << finding.claimA
                  << (finding.claimB.empty() ? "" : " vs " + finding.claimB) << "\n";
        for (const auto& e : finding.excerpts) {
            std::cout << "  " << e.documentId << " [" << e.start << "," << e.end << ") \""
                      << e.quote << "\"\n";
        }
        for (const auto& step : finding.derivation) std::cout << "  step: " << step << "\n";
        std::cout << "  benign: " << finding.benignExplanation << "\n";
    }
    return kExitOk;
}

int cmdCircular() {
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto groups = verity::engine::ProvenanceTracer::circularGroups(engine.records());
    if (groups.empty()) {
        std::cout << "no circular sourcing detected\n";
        return kExitOk;
    }
    for (const auto& group : groups) {
        std::cout << "Potential contradiction (circular sourcing) witness:";
        for (const auto& doc : group.witness) std::cout << " " << doc;
        std::cout << "\n";
        for (const auto& [from, to] : group.edges) {
            std::cout << "  " << from << " -> " << to << "\n";
        }
    }
    return kExitOk;
}

int cmdKeyEntities() {
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto top = verity::engine::Ranker::keyEntities(engine.records(), engine.store(),
                                                        engine.graph(), 5);
    std::cout << "key entities (by graph degree, noisy-OR evidence score):\n";
    for (const auto& score : top) {
        std::cout << "  " << score.entityId << " \"" << score.label << "\" degree "
                  << score.degree << " evidence " << score.evidenceScore << "\n";
    }
    return kExitOk;
}

int cmdBackbone() {
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto edges = verity::engine::Ranker::backbone(engine.graph());
    std::cout << "MST backbone (Kruskal, undirected projection, cost 1 - confidence):\n";
    for (const auto& [a, b, w] : edges) {
        std::cout << "  " << a << " -- " << b << " cost " << w << "\n";
    }
    std::cout << "note: the minimized quantity is the defined additive cost, "
                 "not a truth/reliability score\n";
    return kExitOk;
}

int cmdPath(const Args& args) {
    if (args.positional.size() != 2) {
        std::cerr << "usage: verity path <from-id> <to-id>\n";
        return kExitUsage;
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const verity::model::NodeId from(args.positional[0]);
    const verity::model::NodeId to(args.positional[1]);
    if (!engine.graph().hasNode(from) || !engine.graph().hasNode(to)) {
        std::cerr << "error: unknown node id\n";
        return kExitDataError;
    }
    const double distance = verity::ds::dijkstraDistance(
        engine.graph(), from, to, [](const verity::engine::EdgeAttr& attr) {
            return 1.0 - attr.weight;
        });
    if (std::isinf(distance)) {
        std::cout << "unreachable\n";
        return kExitOk;
    }
    std::cout << "min additive cost (" << from.str() << " -> " << to.str() << "): " << distance
              << "\n";
    std::cout << "note: cost = sum of (1 - confidence) edges; not a truth score\n";
    return kExitOk;
}

int cmdTopEvidence(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity top-evidence <claim-id>\n";
        return kExitUsage;
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto resolution = verity::engine::EntityResolver::resolve(engine.records());
    const auto timeline = verity::engine::TimelineBuilder::build(engine.records());
    const auto context = verity::engine::ConflictDetector::buildContext(
        engine.records(), resolution, timeline);
    verity::engine::ConflictDetector detector;
    std::ifstream rulesFile("data/rules/rules.txt");
    std::string line;
    while (rulesFile && std::getline(rulesFile, line)) {
        auto rule = verity::engine::ConflictDetector::RuleFactory::fromText(line);
        if (rule) detector.addRule(std::move(rule));
    }
    const auto conflicts = detector.run(context);
    const auto bundle = verity::engine::EvidenceBuilder::build(engine.records(), conflicts);

    const std::string& claimId = args.positional[0];
    std::vector<const verity::model::IScorable*> mine;
    std::vector<verity::model::Evidence*> ordered;
    for (const auto& e : bundle.supporting) ordered.push_back(e);
    for (const auto& e : bundle.contradicting) ordered.push_back(e);
    for (const auto& e : bundle.neutral) ordered.push_back(e);
    for (const auto* e : ordered) {
        if (e->claimId().str() == claimId) mine.push_back(e);
    }
    const auto top = verity::engine::Ranker::topEvidence(mine, 5);
    std::cout << "top evidence for " << claimId << ":\n";
    for (const auto* e : top) {
        const auto* base = dynamic_cast<const verity::model::Evidence*>(e);
        std::cout << "  " << base->kindName() << " @" << base->documentId().str() << " score "
                  << e->score() << "\n";
    }
    return kExitOk;
}

int cmdTrace(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity trace <claim-id>\n";
        return kExitUsage;
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto tree =
        verity::engine::ProvenanceTracer::originTreeOf(engine.records(), args.positional[0]);
    if (tree.rootDocument.empty()) {
        std::cerr << "error: unknown claim id\n";
        return kExitDataError;
    }
    std::cout << "origin tree for " << tree.claimId << " (root " << tree.rootDocument
              << ", cyclic origin: " << (tree.reachableWithoutCycle ? "no" : "yes") << ")\n";
    for (const auto& [from, to] : tree.treeEdges) {
        std::cout << "  tree   " << from << " -> " << to << "\n";
    }
    for (const auto& [from, to] : tree.extraEdges) {
        std::cout << "  extra  " << from << " -> " << to << "\n";
    }
    std::cout << "note: extra edges are retained so the provenance graph is never "
                 "presented as a tree\n";
    return kExitOk;
}

int cmdDmLogic() {
    const auto formula = verity::dm::parseFormula("!(p && q) <-> (!p || !q)");
    std::cout << "formula: " << verity::dm::printInfix(*formula) << "\n";
    std::cout << "prefix:  " << verity::dm::printPrefix(*formula) << "\n";
    std::cout << "postfix: " << verity::dm::printPostfix(*formula) << "\n";
    std::cout << "tautology (De Morgan): " << verity::dm::isTautology(*formula) << "\n";
    const auto a = verity::dm::parseFormula("p -> q");
    const auto b = verity::dm::parseFormula("!q -> !p");
    std::cout << "contrapositive equivalence: "
              << verity::dm::semanticallyEquivalent(*a, *b) << "\n";
    const auto cnf = verity::dm::toCNF(*a);
    std::cout << "CNF(p -> q): " << cnf.size() << " clause(s)\n";
    return kExitOk;
}

int cmdDmGraphs() {
    verity::ds::Graph<std::string, double> g(false);
    for (const char* n : {"A", "B", "C", "D", "E"}) g.addNode(n);
    g.addEdgeUndirected("A", "B", 1);
    g.addEdgeUndirected("B", "C", 1);
    g.addEdgeUndirected("C", "D", 1);
    g.addEdgeUndirected("D", "A", 1);
    g.addEdgeUndirected("B", "D", 1);
    std::cout << "handshake: " << verity::dm::handshakeHolds(g) << "  edges "
              << g.edgeCount() << "\n";
    std::cout << "bipartite: " << (verity::dm::bipartiteColoring(g).empty() ? "no" : "yes")
              << " (odd cycle B-C-D-B)\n";
    std::cout << "Euler type: " << verity::dm::eulerType(g) << "\n";
    const auto colors = verity::dm::greedyColoring(g);
    std::cout << "greedy coloring: ";
    for (const auto& [node, color] : colors) std::cout << node << "=" << color << " ";
    std::cout << "\nwalks A->D length 2: "
              << verity::dm::walkCount(g, std::string("A"), std::string("D"), 2) << "\n";
    std::cout << "walks A->D length 3: "
              << verity::dm::walkCount(g, std::string("A"), std::string("D"), 3) << "\n";
    const auto cuts = verity::ds::cutVerticesAndBridges(g);
    std::cout << "articulation points:";
    for (const auto& n : cuts.articulationPoints) std::cout << " " << n;
    std::cout << "\nbridges: " << cuts.bridges.size() << "\n";
    return kExitOk;
}

'''
src = src.replace(marker, new_cmds + marker)

src = src.replace(
    '        if (command == "timeline") return cmdTimeline();',
    '        if (command == "timeline") return cmdTimeline();\n'
    '        if (command == "conflicts") return cmdConflicts(args);\n'
    '        if (command == "circular") return cmdCircular();\n'
    '        if (command == "keyentities") return cmdKeyEntities();\n'
    '        if (command == "backbone") return cmdBackbone();\n'
    '        if (command == "path") return cmdPath(args);\n'
    '        if (command == "top-evidence") return cmdTopEvidence(args);\n'
    '        if (command == "trace") return cmdTrace(args);')
src = src.replace(
    '''            if (!args.positional.empty() && args.positional[0] == "combinatorics") {
                return cmdDmCombinatorics();
            }
            return cmdDm(args);''',
    '''            if (!args.positional.empty() && args.positional[0] == "combinatorics") {
                return cmdDmCombinatorics();
            }
            if (!args.positional.empty() && args.positional[0] == "logic") return cmdDmLogic();
            if (!args.positional.empty() && args.positional[0] == "graphs") return cmdDmGraphs();
            return cmdDm(args);''')

src = src.replace('#include <cstdlib>', '#include <cmath>\n#include <cstdlib>')

open(p, "w", encoding="utf-8", newline="\n").write(src)
print("main.cpp patched")
