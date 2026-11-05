#include "verity/engine/Exporter.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <set>
#include <sstream>

#include "verity/dm/Ledger.hpp"
#include "verity/engine/ConflictDetector.hpp"
#include "verity/engine/EntityResolver.hpp"
#include "verity/engine/Reasoning.hpp"
#include "verity/engine/TimelineBuilder.hpp"
#include "verity/model/ClaimNode.hpp"
#include "verity/model/DocumentNode.hpp"
#include "verity/model/Entity.hpp"
#include "verity/model/EventNode.hpp"
#include "verity/model/NodeId.hpp"
#include "verity/util/VerityError.hpp"

namespace verity::engine {

namespace {

std::string isoTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

nlohmann::json claimJson(const model::ClaimRecord& claim) {
    nlohmann::json j;
    j["id"] = claim.id;
    j["subject_id"] = claim.subject_id;
    j["predicate"] = claim.predicate;
    j["object"] = {{"kind", claim.object.kind == model::ClaimObject::Kind::EntityRef ? "EntityRef"
                             : claim.object.kind == model::ClaimObject::Kind::DateValue ? "DateValue"
                             : claim.object.kind == model::ClaimObject::Kind::QuantityValue
                                 ? "QuantityValue"
                                 : "StringValue"},
                   {"text", claim.object.text},
                   {"entity_id", claim.object.entity_id},
                   {"date", claim.object.kind == model::ClaimObject::Kind::DateValue
                                ? nlohmann::json(claim.object.date.toString())
                                : nlohmann::json(nullptr)},
                   {"quantity", claim.object.kind == model::ClaimObject::Kind::QuantityValue
                                    ? claim.object.quantity
                                    : 0.0},
                   {"unit", claim.object.unit}};
    j["polarity"] = claim.polarity;
    j["confidence"] = claim.confidence;
    j["document_id"] = claim.document_id;
    j["span"] = {{"start", claim.span.start}, {"end", claim.span.end},
                 {"quote", claim.span.quote}};
    j["valid_from"] = claim.valid_from ? nlohmann::json(*claim.valid_from) : nlohmann::json(nullptr);
    j["valid_to"] = claim.valid_to ? nlohmann::json(*claim.valid_to) : nlohmann::json(nullptr);
    return j;
}

nlohmann::json conflictsJson(const std::vector<Conflict>& findings) {
    nlohmann::json arr = nlohmann::json::array();
    for (const Conflict& f : findings) {
        nlohmann::json j;
        j["id"] = f.id;
        j["rule"] = f.rule;
        j["statement"] = f.statement;
        j["claim_a"] = f.claimA;
        j["claim_b"] = f.claimB;
        j["subject_id"] = f.subjectId;
        j["predicate"] = f.predicate;
        j["documents"] = f.documentIds;
        j["excerpts"] = nlohmann::json::array();
        for (const Conflict::Excerpt& e : f.excerpts) {
            j["excerpts"].push_back({{"document_id", e.documentId},
                                     {"start", e.start},
                                     {"end", e.end},
                                     {"quote", e.quote}});
        }
        j["derivation"] = f.derivation;
        j["severity"] = f.severity;
        j["confidence"] = f.confidence;
        j["benign_explanation"] = f.benignExplanation;
        arr.push_back(std::move(j));
    }
    return arr;
}

} // namespace

nlohmann::json Exporter::buildReport(const VerityEngine& engine, const Options& options) {
    nlohmann::json report;
    report["schema_version"] = "1.0.0";
    report["metadata"] = {
        {"generated", isoTimestamp()},
        {"dataset", options.datasetLabel},
        {"wording", "Potential contradiction (never a truth verdict)"},
        {"generator", "verity engine 1.0.0"},
    };

    // ---- nodes + edges from the evidence graph ------------------------------
    const auto& graph = engine.graph();
    report["nodes"] = nlohmann::json::array();
    for (const auto* node : engine.store().all()) {
        nlohmann::json n = node->toJson();
        n["kind"] = node->type();
        report["nodes"].push_back(std::move(n));
    }
    report["edges"] = nlohmann::json::array();
    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& from : graph.nodes()) {
        for (const auto& [to, attr] : graph.neighbors(from)) {
            const std::string fromId = from.str();
            const std::string toId = to.str();
            const std::pair<std::string, std::string> canonical =
                fromId < toId ? std::make_pair(fromId, toId)
                              : std::make_pair(toId, fromId);
            if (!graph.directed() && !seen.insert(canonical).second) continue;
            if (graph.directed() && !seen.insert({fromId, toId}).second) continue;
            report["edges"].push_back({{"from", fromId},
                                       {"to", toId},
                                       {"relation", std::string(to_string(attr.relation))},
                                       {"weight", attr.weight},
                                       {"label", attr.label}});
        }
    }

    // ---- documents + claims -------------------------------------------------
    report["documents"] = nlohmann::json::array();
    for (const auto& doc : engine.records().documents) {
        report["documents"].push_back({{"id", doc.id},
                                       {"filename", doc.filename},
                                       {"format", doc.format},
                                       {"normalized_text", doc.normalized_text},
                                       {"citations", doc.citations}});
    }
    report["claims"] = nlohmann::json::array();
    for (const auto& claim : engine.records().claims) {
        report["claims"].push_back(claimJson(claim));
    }

    // ---- detection + resolution + timeline ----------------------------------
    if (options.runDetection) {
        const auto resolution = EntityResolver::resolve(engine.records());
        const auto timeline = TimelineBuilder::build(engine.records());
        const auto context = ConflictDetector::buildContext(engine.records(), resolution, timeline);

        ConflictDetector detector;
        std::ifstream rulesFile(options.rulesPath);
        std::string line;
        while (rulesFile && std::getline(rulesFile, line)) {
            auto rule = ConflictDetector::RuleFactory::fromText(line);
            if (rule) detector.addRule(std::move(rule));
        }
        const auto findings = detector.run(context);
        report["conflicts"] = conflictsJson(findings);
        report["entity_resolution"] = {
            {"merged_pairs", resolution.mergedPairs},
            {"classes", [&resolution] {
                 nlohmann::json arr = nlohmann::json::array();
                 for (const auto& cls : resolution.classes) arr.push_back(cls);
                 return arr;
             }()},
        };
        nlohmann::json timelineJson;
        if (timeline.finding.has_value()) {
            timelineJson["finding"] = {{"statement", timeline.finding->statement},
                                       {"witness", timeline.finding->witness},
                                       {"claims", timeline.finding->claimIds}};
        } else {
            timelineJson["finding"] = nullptr;
            timelineJson["chronological_order"] = timeline.chronologicalOrder;
            timelineJson["topological_order"] = timeline.topologicalOrder;
            timelineJson["hasse_edges"] = nlohmann::json::array();
            for (const auto& [a, b] : timeline.hasseEdges) {
                timelineJson["hasse_edges"].push_back({{"from", a}, {"to", b}});
            }
            timelineJson["hasse_dot"] = timeline.hasseDot;
            timelineJson["incomparable_pairs"] = nlohmann::json::array();
            for (const auto& [a, b] : timeline.incomparablePairs) {
                timelineJson["incomparable_pairs"].push_back({{"a", a}, {"b", b}});
            }
        }
        report["timeline"] = std::move(timelineJson);
    } else {
        report["conflicts"] = nlohmann::json::array();
        report["timeline"] = nlohmann::json::object();
    }

    // ---- provenance ----------------------------------------------------------
    const auto groups = ProvenanceTracer::circularGroups(engine.records());
    nlohmann::json provenance;
    provenance["circular_groups"] = nlohmann::json::array();
    for (const auto& group : groups) {
        provenance["circular_groups"].push_back({{"witness", group.witness},
                                                 {"edges", [&group] {
                                                      nlohmann::json arr =
                                                          nlohmann::json::array();
                                                      for (const auto& [a, b] : group.edges) {
                                                          arr.push_back({{"from", a},
                                                                         {"to", b}});
                                                      }
                                                      return arr;
                                                  }()}});
    }
    report["provenance"] = std::move(provenance);

    // ---- rankings -------------------------------------------------------------
    const auto keyEntities = Ranker::keyEntities(engine.records(), engine.store(), graph, 8);
    report["rankings"] = {{"key_entities", [&keyEntities] {
                               nlohmann::json arr = nlohmann::json::array();
                               for (const auto& s : keyEntities) {
                                   arr.push_back({{"entity_id", s.entityId},
                                                  {"label", s.label},
                                                  {"degree", s.degree},
                                                  {"evidence_score", s.evidenceScore}});
                               }
                               return arr;
                           }()}};

    // ---- ledger (teaching-grade) ---------------------------------------------
    report["ledger"] = buildLedgerJson(engine);

    // ---- dm demonstrations (precomputed engine examples) ---------------------
    report["dm"] = {
        {"note", "Precomputed engine examples; the DM Lab recomputes interactively"},
        {"sets_inclusion_exclusion", {{"a", {1, 2, 3, 4}}, {"b", {3, 4, 5}},
                                      {"union_size_ie", 5}}},
        {"numtheory", {{"gcd_240_46", 2}, {"phi_12", 4}, {"nextprime_64", 67}}},
        {"logic_de_morgan_tautology", true},
    };

    return report;
}

std::string Exporter::toDot(const VerityEngine& engine) {
    const auto& graph = engine.graph();
    std::ostringstream dot;
    dot << "digraph verity {\n  rankdir=LR;\n  node [fontname=\"Helvetica\"];\n";
    for (const auto* node : engine.store().all()) {
        const std::string shape = node->type() == "Entity"     ? "box"
                                  : node->type() == "Claim"    ? "note"
                                  : node->type() == "Event"    ? "ellipse"
                                                               : "folder";
        dot << "  \"" << node->id().str() << "\" [label=\"" << node->id().str() << " "
            << node->label() << "\", shape=" << shape << "];\n";
    }
    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& from : graph.nodes()) {
        for (const auto& [to, attr] : graph.neighbors(from)) {
            const std::pair<std::string, std::string> key =
                from.str() < to.str() ? std::make_pair(from.str(), to.str())
                                      : std::make_pair(to.str(), from.str());
            if (!graph.directed() && !seen.insert(key).second) continue;
            if (graph.directed() && !seen.insert({from.str(), to.str()}).second) continue;
            dot << "  \"" << from.str() << "\" -> \"" << to.str() << "\" [label=\""
                << attr.label << "\"];\n";
        }
    }
    dot << "}\n";
    return dot.str();
}

nlohmann::json Exporter::buildLedgerJson(const VerityEngine& engine) {
    verity::dm::EvidenceLedger ledger;
    nlohmann::json entriesData = nlohmann::json::array();
    for (const auto& claim : engine.records().claims) {
        const std::string record = claim.id + "|" + claim.subject_id + "|" +
                                   claim.predicate + "|" + claim.document_id;
        ledger.append(record);
        entriesData.push_back(record);
    }
    nlohmann::json j;
    j["mode"] = "teaching-grade modular hash chain (NOT cryptographic integrity)";
    j["entries"] = ledger.size();
    j["entries_data"] = entriesData;
    j["head"] = ledger.size() > 0 ? std::to_string(ledger.entries().back().hash) : "0";
    const bool verified = ledger.verify() == -1;
    j["verified"] = verified;
    j["verification_time"] = isoTimestamp();
    j["note"] = "loading this JSON does NOT re-verify the binary ledger; run "
                "`verity verify-ledger` for an actual verification";
    return j;
}

void Exporter::saveLedger(const nlohmann::json& ledger, const std::string& path) {
    std::ofstream out(path);
    if (!out) throw util::StorageError("cannot open ledger file: " + path);
    out << ledger.dump(2) << "\n";
    if (!out) throw util::StorageError("ledger write failed: " + path);
}

nlohmann::json Exporter::loadLedger(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw util::StorageError("cannot open ledger file: " + path);
    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::parse_error& e) {
        throw util::StorageError(std::string("corrupt ledger JSON: ") + e.what());
    }
    return j;
}

bool Exporter::verifyLedgerJson(const nlohmann::json& ledger, long& firstBad) {
    // recompute the chain over the ledger's stored entries (teaching-grade)
    if (!ledger.contains("entries") || !ledger.contains("entries_data")) return false;
    verity::dm::EvidenceLedger rebuilt;
    for (const auto& record : ledger.at("entries_data")) {
        rebuilt.append(record.get<std::string>());
    }
    firstBad = rebuilt.verify();
    return firstBad == -1;
}

std::string Exporter::toHtml(const nlohmann::json& report, const std::string& visNetworkJsPath) {
    std::ifstream visFile(visNetworkJsPath, std::ios::binary);
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">\n"
         << "<title>Verity Graph — Evidence Observatory</title>\n"
         << "<style>body{background:#0B0D12;color:#F4F1E9;font-family:system-ui;"
         << "margin:0;padding:24px}h1{color:#D7FF3F}</style>\n</head><body>\n"
         << "<h1>VERITY — Evidence Observatory</h1>\n"
         << "<p>Follow the evidence. Explore connections, inspect sources, and "
         << "investigate potential contradictions.</p>\n"
         << "<p><em>Fictional demo dataset. Findings are "
         << "&ldquo;Potential contradiction&rdquo; labels — never truth verdicts.</em></p>\n"
         << "<div id=\"graph\" style=\"width:100%;height:560px;border:1px solid #141923;"
         << "border-radius:8px\"></div>\n"
         << "<div id=\"summary\"></div>\n";
    if (visFile) {
        std::ostringstream vis;
        vis << visFile.rdbuf();
        std::string visSource = vis.str();
        // escape embedded script terminators (defense against broken markup)
        const std::string needle = "</script";
        std::size_t pos = 0;
        while ((pos = visSource.find(needle, pos)) != std::string::npos) {
            visSource.replace(pos, needle.size(), "<\\/script");
            pos += needle.size();
        }
        html << "<script>\n" << visSource << "\n</script>\n";
    } else {
        html << "<p><strong>vis-network bundle not embedded ("
             << visNetworkJsPath << " missing) — graph canvas disabled.</strong></p>\n";
    }
    std::string reportJson = report.dump();
    const std::string needle = "</script";
    std::size_t pos = 0;
    while ((pos = reportJson.find(needle, pos)) != std::string::npos) {
        reportJson.replace(pos, needle.size(), "<\\/script");
        pos += needle.size();
    }
    html << "<script id=\"verity-report\" type=\"application/json\">"
         << reportJson << "</script>\n";
    html << "<script>\n"
         << "(function(){\n"
         << "  var el = document.getElementById('verity-report');\n"
         << "  var report = JSON.parse(el.textContent);\n"
         << "  var s = document.getElementById('summary');\n"
         << "  s.innerHTML = '<h2>' + report.nodes.length + ' nodes, ' +\n"
         << "    report.edges.length + ' edges, ' +\n"
         << "    (report.conflicts ? report.conflicts.length : 0) +\n"
         << "    ' potential contradictions</h2>';\n"
         << "  if (typeof vis !== 'undefined') {\n"
         << "    var nodes = new vis.DataSet(report.nodes.map(function(n){\n"
         << "      return {id: n.id, label: n.label || n.id, group: n.kind};}));\n"
         << "    var edges = new vis.DataSet(report.edges.map(function(e){\n"
         << "      return {from: e.from, to: e.to, label: e.label || ''};}));\n"
         << "    new vis.Network(document.getElementById('graph'),\n"
         << "      {nodes: nodes, edges: edges},\n"
         << "      {groups: {Entity:{color:'#50DCE8'}, Claim:{color:'#D7FF3F'},\n"
         << "        Event:{color:'#B8A0FF'}, Document:{color:'#FF7D75'}}});\n"
         << "  }\n"
         << "})();\n</script>\n</body></html>\n";
    return html.str();
}

} // namespace verity::engine
