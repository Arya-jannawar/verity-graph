// VERITY GRAPH — tests/engine/test_conflicts.cpp
// Phase-3 reasoning: planted conflicts with derivations and spans, benign
// cases NOT flagged, rule factory grammar, Observer listeners, a NEW rule
// integrating WITHOUT modifying ConflictDetector, exception hierarchy tests,
// circular sourcing, intermediary disconnection.

#include "doctest/doctest.h"

#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <vector>

#include "verity/engine/ConflictDetector.hpp"
#include "verity/engine/Reasoning.hpp"
#include "verity/io/JsonLoader.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity::engine;

namespace {

/// Loads and merges the nine gold fragments (repo data, no generated files)
/// exactly like the Python MockExtractor does, then validates the merged root.
verity::model::ExtractionRecord loadAll() {
    static const char* files[] = {
        "data/gold/d001.gold.json", "data/gold/d002.gold.json", "data/gold/d003.gold.json",
        "data/gold/d004.gold.json", "data/gold/d005.gold.json", "data/gold/d006.gold.json",
        "data/gold/d007.gold.json", "data/gold/d008.gold.json", "data/gold/d009.gold.json"};
    nlohmann::json merged;
    merged["schema_version"] = "1.0.0";
    merged["documents"] = nlohmann::json::array();
    merged["claims"] = nlohmann::json::array();
    merged["events"] = nlohmann::json::array();
    merged["citations"] = nlohmann::json::array();
    // entities merge BY ID (aliases/mentions append) exactly like MockExtractor
    std::map<std::string, nlohmann::json> entityById;
    std::vector<std::string> entityOrder;
    for (const char* file : files) {
        std::ifstream in(file);
        if (!in) throw verity::util::VerityError(std::string("missing gold file: ") + file);
        nlohmann::json fragment;
        in >> fragment;
        for (const auto& d : fragment["documents"]) merged["documents"].push_back(d);
        for (const auto& c : fragment["claims"]) merged["claims"].push_back(c);
        for (const auto& e : fragment["events"]) merged["events"].push_back(e);
        for (const auto& c : fragment["citations"]) merged["citations"].push_back(c);
        for (const auto& e : fragment["entities"]) {
            const std::string id = e.at("id").get<std::string>();
            if (entityById.find(id) == entityById.end()) {
                entityById[id] = e;
                entityOrder.push_back(id);
            } else {
                for (const auto& alias : e.at("aliases")) {
                    entityById[id]["aliases"].push_back(alias);
                }
                for (const auto& m : e.at("mentions")) {
                    entityById[id]["mentions"].push_back(m);
                }
            }
        }
    }
    for (const std::string& id : entityOrder) merged["entities"].push_back(entityById[id]);
    return verity::io::JsonLoader::parseExtraction(merged.dump());
}

struct FullContext {
    verity::model::ExtractionRecord records;
    ResolutionResult resolution;
    TimelineResult timeline;
    DetectionContext context;

    static FullContext make() {
        FullContext fc;
        fc.records = loadAll();
        fc.resolution = EntityResolver::resolve(fc.records);
        fc.timeline = TimelineBuilder::build(fc.records);
        fc.context = ConflictDetector::buildContext(fc.records, fc.resolution, fc.timeline);
        return fc;
    }
};

std::unique_ptr<ConflictDetector> defaultDetector() {
    auto detector = std::make_unique<ConflictDetector>();
    std::ifstream rulesFile("data/rules/rules.txt");
    std::string line;
    while (rulesFile && std::getline(rulesFile, line)) {
        auto rule = ConflictDetector::RuleFactory::fromText(line);
        if (rule) detector->addRule(std::move(rule));
    }
    return detector;
}

const Conflict* findRule(const std::vector<Conflict>& findings, const std::string& rule,
                         const std::string& claimA) {
    for (const auto& f : findings) {
        if (f.rule == rule && f.claimA == claimA) return &f;
    }
    return nullptr;
}

} // namespace

TEST_CASE("planted conflicts appear with rules, claims, spans and benign explanations") {
    const auto fc = FullContext::make();
    const auto findings = defaultDetector()->run(fc.context);

    CHECK_FALSE(findings.empty());
    // every finding statement uses the frozen wording (temporal findings use
    // the "Potential temporal contradiction" phrase)
    for (const auto& f : findings) {
        const bool frozenWording =
            f.statement.find("Potential contradiction") != std::string::npos ||
            f.statement.find("Potential temporal contradiction") != std::string::npos;
        CHECK(frozenWording);
        CHECK_FALSE(f.benignExplanation.empty());
        const bool validSeverity = (f.severity == "low" || f.severity == "medium" ||
                                    f.severity == "high");
        CHECK(validSeverity);
        const bool wellFormedId = f.id == "X001" || f.id.size() == 4;
        CHECK(wellFormedId);   // X001-style ids
    }

    // planted date conflict: C010 (March 10) vs C014 (June 5) — disjoint dates
    bool dateConflict = false;
    for (const auto& f : findings) {
        if ((f.claimA == "C010" && f.claimB == "C014") ||
            (f.claimA == "C014" && f.claimB == "C010")) {
            dateConflict = true;
            CHECK(f.rule == "FunctionalPredicateRule");
            REQUIRE_EQ(f.excerpts.size(), 2u);
            CHECK_EQ(f.excerpts[0].quote, "Product X launched in Bellhaven on 2024-03-10");
            CHECK_EQ(f.excerpts[1].quote,
                     "Aster Vale Systems launched Product X in Crescent Bay on 2024-06-05");
        }
    }
    CHECK(dateConflict);

    // planted negation conflict: C014 positive vs C017 negative, same date
    bool negation = false;
    for (const auto& f : findings) {
        if (f.rule == "NegationRule" &&
            ((f.claimA == "C014" && f.claimB == "C017") ||
             (f.claimA == "C017" && f.claimB == "C014"))) {
            negation = true;
        }
    }
    CHECK(negation);

    // planted CEO conflict: C015 (Priya, valid 2023-06) vs C020 (Maria, 2022-01)
    bool ceo = false;
    for (const auto& f : findings) {
        if (f.predicate == "has_ceo") ceo = true;
    }
    CHECK(ceo);

    // planted alibi conflict: C012 vs C019 same-day different places
    bool alibi = false;
    for (const auto& f : findings) {
        if (f.rule == "LocationTimeRule") alibi = true;
    }
    CHECK(alibi);

    // planted numeric mismatch: C013 (5 modules) vs C016 (8 modules)
    bool numeric = false;
    for (const auto& f : findings) {
        if (f.rule == "NumericMismatchRule") numeric = true;
    }
    CHECK(numeric);

    // planted temporal cycle: EV001 -> EV002 -> EV003 -> EV001
    bool temporal = false;
    for (const auto& f : findings) {
        if (f.rule == "TemporalCycleRule") {
            temporal = true;
            CHECK(f.statement.find("Potential temporal contradiction") != std::string::npos);
            CHECK_FALSE(f.derivation.empty());
            CHECK(f.derivation[0].find("EV001 -> EV002 -> EV003 -> EV001") != std::string::npos);
        }
    }
    CHECK(temporal);
}

TEST_CASE("benign cases are NOT classified as findings") {
    const auto fc = FullContext::make();
    const auto findings = defaultDetector()->run(fc.context);

    for (const auto& f : findings) {
        // announced (C011) never conflicts with launched claims
        const bool c011Pair = f.claimA == "C011" && f.claimB != "";
        CHECK_FALSE(c011Pair);
        // the month-vs-day announced pair (C011 2024-03 vs C021 2024-03-15)
        // must NOT appear — different precision but overlapping interval
        const bool monthVsDay = (f.claimA == "C011" && f.claimB == "C021") ||
                                (f.claimA == "C021" && f.claimB == "C011");
        CHECK_FALSE(monthVsDay);
    }
    // C018 re-states March 10 (same value as C010) — consistent, no clash
    for (const auto& f : findings) {
        const bool restatedMarch = (f.claimA == "C010" && f.claimB == "C018") ||
                                   (f.claimA == "C018" && f.claimB == "C010");
        CHECK_FALSE(restatedMarch);
    }
}

TEST_CASE("a NEW ConflictRule integrates WITHOUT modifying ConflictDetector") {
    /// Local rule defined entirely in this test file.
    class SuspiciousIdRule final : public ConflictRule {
    public:
        SuspiciousIdRule() : ConflictRule("SuspiciousIdRule") {}
        void detect(const DetectionContext& context,
                    std::vector<Conflict>& findings) const override {
            for (const auto& [key, claimIds] : context.claimIndex) {
                (void)key;
                for (const std::string& id : claimIds) {
                    if (id == "C013") {
                        const auto* claim = context.claimById.at(id);
                        Conflict finding;
                        finding.rule = name();
                        finding.statement = "Potential contradiction (test rule)";
                        finding.claimA = id;
                        finding.subjectId = claim->subject_id;
                        finding.predicate = claim->predicate;
                        finding.severity = "low";
                        finding.benignExplanation = "test-only rule";
                        findings.push_back(std::move(finding));
                    }
                }
            }
        }
    };

    const auto fc = FullContext::make();
    auto detector = defaultDetector();
    detector->addRule(std::make_unique<SuspiciousIdRule>());   // no detector change!
    const auto findings = detector->run(fc.context);
    const Conflict* testFinding = findRule(findings, "SuspiciousIdRule", "C013");
    REQUIRE(testFinding != nullptr);
    CHECK(testFinding->statement.find("Potential contradiction") != std::string::npos);
}

TEST_CASE("RuleFactory grammar violations throw RuleError; comments pass") {
    CHECK(ConflictDetector::RuleFactory::fromText("") == nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("# comment") == nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("functional has_ceo overlap") != nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("negation any overlap") != nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("numeric any same_unit") != nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("location present_at overlap") != nullptr);
    CHECK(ConflictDetector::RuleFactory::fromText("temporal_cycle happened_before") != nullptr);
    CHECK_THROWS_AS((void)ConflictDetector::RuleFactory::fromText("evil eval p"),
                    verity::util::RuleError);
    CHECK_THROWS_AS((void)ConflictDetector::RuleFactory::fromText("functional"),
                    verity::util::RuleError);
    CHECK_THROWS_AS((void)ConflictDetector::RuleFactory::fromText("bogus x y"),
                    verity::util::RuleError);
}

TEST_CASE("Observer listeners receive every finding") {
    const auto fc = FullContext::make();
    auto detector = defaultDetector();
    ConflictJsonCollector collector;
    ConflictJsonCollector second;
    detector->addListener(&collector);
    detector->addListener(&second);
    const auto findings = detector->run(fc.context);
    CHECK_EQ(collector.findings().size(), findings.size());
    CHECK_EQ(second.findings().size(), findings.size());
    for (std::size_t i = 0; i < findings.size(); ++i) {
        CHECK_EQ(collector.findings()[i].id, findings[i].id);
    }
}

TEST_CASE("FixedCache: LRU eviction with non-type capacity") {
    FixedCache<std::string, int, 2> cache;
    cache.put("a", 1);
    cache.put("b", 2);
    CHECK_EQ(*cache.get("a"), 1);
    cache.put("c", 3);                 // evicts LRU "b" (a was refreshed)
    CHECK(cache.get("a").has_value());
    CHECK_FALSE(cache.get("b").has_value());
    CHECK_EQ(*cache.get("c"), 3);
    CHECK_EQ(cache.size(), 2u);
    CHECK_EQ(cache.capacity(), 2u);
}

TEST_CASE("exception hierarchy: catch by base and derived, rethrow") {
    try {
        throw verity::util::MissingNodeError("E999");
    } catch (const verity::util::GraphError& g) {
        CHECK(std::string(g.what()).find("MissingNodeError") != std::string::npos);
        try {
            throw;   // rethrow with throw;
        } catch (const verity::util::VerityError& base) {
            CHECK(std::string(base.what()).find("E999") != std::string::npos);
        }
    }
    // derived caught before base in multiple catch blocks
    try {
        throw verity::util::DuplicateNodeError("E001");
    } catch (const verity::util::DuplicateNodeError&) {
        CHECK(true);
    } catch (const verity::util::GraphError&) {
        CHECK_MESSAGE(false, "derived handler should win");
    }
    CHECK_THROWS_AS((void)ConflictDetector::RuleFactory::fromText("junk"), verity::util::RuleError);
}

TEST_CASE("circular sourcing: D006 -> D007 -> D008 -> D006") {
    const auto records = loadAll();
    const auto groups = ProvenanceTracer::circularGroups(records);
    REQUIRE_EQ(groups.size(), 1u);
    const auto& group = groups[0];
    REQUIRE_EQ(group.witness.size(), 4u);
    CHECK_EQ(group.witness.front(), group.witness.back());
    std::set<std::string> docs(group.witness.begin(), group.witness.end());
    CHECK_EQ(docs, std::set<std::string>({"D006", "D007", "D008"}));

    // origin tree retains extra (cyclic) edges
    const auto tree = ProvenanceTracer::originTreeOf(records, "C010");
    CHECK_EQ(tree.rootDocument, "D006");
    CHECK_FALSE(tree.treeEdges.empty());
    CHECK_FALSE(tree.extraEdges.empty());   // the cycle edge stays visible
    CHECK_FALSE(tree.reachableWithoutCycle);

    // claims sourced in the cycle are flagged
    CHECK(ProvenanceTracer::claimInCircularGroup(records, "C010"));
    CHECK_FALSE(ProvenanceTracer::claimInCircularGroup(records, "C001"));
}

TEST_CASE("the planted intermediary disconnects the co-mention projection") {
    const auto fc = FullContext::make();
    // Projection (documented): the ENTITY CO-MENTION network over the
    // phase-3 documents D006-D009 — two entities are adjacent iff they share
    // a mention document. Elena Vasquez (E009) is the ONLY entity mentioned
    // both in the Aster Vale documents (D006) and the board memo (D009);
    // removing her must split {D006,D007,D008 entities} from {D009 entities}.

    auto components = [&](const verity::model::ExtractionRecord& records,
                          const std::set<std::string>& docScope,
                          const std::set<std::string>& removed) {
        std::map<std::string, std::set<std::string>> adjacency;
        for (const auto& entity : records.entities) {
            if (removed.count(entity.id) != 0u) continue;
            for (const auto& mention : entity.mentions) {
                if (docScope.count(mention.document_id) == 0u) continue;
                for (const auto& other : records.entities) {
                    if (other.id == entity.id || removed.count(other.id) != 0u) continue;
                    const bool shares = std::any_of(
                        other.mentions.begin(), other.mentions.end(),
                        [&](const verity::model::Mention& m) {
                            return m.document_id == mention.document_id;
                        });
                    if (shares) {
                        adjacency[entity.id].insert(other.id);
                        adjacency[other.id].insert(entity.id);
                    }
                }
            }
        }
        std::size_t count = 0;
        std::set<std::string> seen;
        for (const auto& [entity, neighbors] : adjacency) {
            if (seen.count(entity) != 0u) continue;
            ++count;
            std::vector<std::string> frontier{entity};
            seen.insert(entity);
            while (!frontier.empty()) {
                const std::string node = frontier.back();
                frontier.pop_back();
                for (const std::string& next : adjacency[node]) {
                    if (seen.count(next) == 0u) {
                        seen.insert(next);
                        frontier.push_back(next);
                    }
                }
            }
        }
        return count;
    };

    const std::set<std::string> scope{"D006", "D007", "D008", "D009"};
    const std::size_t withElena = components(fc.records, scope, {});
    const std::size_t withoutElena =
        components(fc.records, scope, std::set<std::string>{"E009"});
    CHECK(withElena >= 1u);
    CHECK(withoutElena > withElena);   // removal disconnects the projection
}
