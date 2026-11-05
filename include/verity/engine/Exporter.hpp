#pragma once
// VERITY GRAPH — engine/Exporter: complete versioned report payload (out/
// graph.json), DOT export and a standalone offline HTML report (Phase 4).
//
// The JSON report is the UI's ONLY data source: every count, finding, date
// and status label in the five tabs derives from this payload (CONTRACT §8).
// Root keys: schema_version, metadata, nodes, edges, documents, claims,
// conflicts, timeline, provenance, rankings, ledger, dm.
//
// The HTML export EMBEDS the report JSON and the vendored vis-network bundle
// so the file works fully offline; embedded scripts escape "</script".
// The frontend never receives instructions — only data.

#include <string>

#include <nlohmann/json.hpp>

#include "verity/engine/VerityEngine.hpp"

namespace verity::engine {

class Exporter {
public:
    struct Options {
        Options() : rulesPath("data/rules/rules.txt"), runDetection(true),
                    datasetLabel("Fictional demo dataset") {}
        std::string rulesPath;
        bool runDetection;
        std::string datasetLabel;
    };

    /// Builds the complete report payload (detection included when enabled).
    [[nodiscard]] static nlohmann::json buildReport(const VerityEngine& engine,
                                                    const Options& options = {});

    /// Graphviz DOT of the evidence graph (typed node shapes, edge labels).
    [[nodiscard]] static std::string toDot(const VerityEngine& engine);

    /// Standalone HTML: report JSON + vendored vis-network embedded inline.
    [[nodiscard]] static std::string toHtml(const nlohmann::json& report,
                                            const std::string& visNetworkJsPath =
                                                "ui/vendor/vis-network.min.js");

    /// Evidence ledger file I/O (teaching-grade; JSON entries + chain head).
    [[nodiscard]] static nlohmann::json buildLedgerJson(const VerityEngine& engine);
    static void saveLedger(const nlohmann::json& ledger, const std::string& path);
    [[nodiscard]] static nlohmann::json loadLedger(const std::string& path);
    [[nodiscard]] static bool verifyLedgerJson(const nlohmann::json& ledger, long& firstBad);
};

} // namespace verity::engine
