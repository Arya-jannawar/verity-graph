#!/usr/bin/env python
"""One-shot development patch: wires Phase-4 CLI commands into app/main.py's
C++ sibling app/main.cpp (export/save/load/verify-ledger). Runs once."""
p = "app/main.cpp"
src = open(p, encoding="utf-8").read()

src = src.replace(
    '#include "verity/engine/Reasoning.hpp"',
    '#include "verity/engine/Exporter.hpp"\n#include "verity/engine/Reasoning.hpp"\n'
    '#include "verity/io/GraphSerializer.hpp"\n#include "verity/dm/Crypto.hpp"\n'
    '#include "verity/dm/Ledger.hpp"\n#include "verity/ds/GraphMatrix.hpp"\n'
    '#include "verity/ds/Trees.hpp"')

src = src.replace(
    '    "  verity trace <claim-id>\\n"\n    "  verity status\\n"',
    '    "  verity trace <claim-id>\\n"\n'
    '    "  verity export [--json p] [--dot p] [--html p]\\n"\n'
    '    "  verity save <graph.vg>\\n"\n'
    '    "  verity load <graph.vg> [--fetch <id>]\\n"\n'
    '    "  verity verify-ledger <ledger.json>\\n"\n'
    '    "  verity status\\n"')

marker = "} // namespace\n\nint main(int argc, char** argv) {"
new_cmds = r'''
int cmdExport(const Args& args) {
    std::string jsonPath, dotPath, htmlPath;
    for (std::size_t i = 0; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--json") jsonPath = args.positional[i + 1];
        else if (args.positional[i] == "--dot") dotPath = args.positional[i + 1];
        else if (args.positional[i] == "--html") htmlPath = args.positional[i + 1];
    }
    if (jsonPath.empty() && dotPath.empty() && htmlPath.empty()) {
        jsonPath = "out/graph.json";
        dotPath = "out/graph.dot";
        htmlPath = "out/report.html";
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto report = verity::engine::Exporter::buildReport(engine);
    if (!jsonPath.empty()) {
        std::ofstream out(jsonPath);
        if (!out) throw verity::util::StorageError("cannot open " + jsonPath);
        out << report.dump(2) << "\n";
        std::cout << "wrote " << jsonPath << " (nodes " << report["nodes"].size()
                  << ", edges " << report["edges"].size() << ", conflicts "
                  << report["conflicts"].size() << ")\n";
    }
    if (!dotPath.empty()) {
        std::ofstream out(dotPath);
        if (!out) throw verity::util::StorageError("cannot open " + dotPath);
        out << verity::engine::Exporter::toDot(engine);
        std::cout << "wrote " << dotPath << "\n";
    }
    if (!htmlPath.empty()) {
        std::ofstream out(htmlPath);
        if (!out) throw verity::util::StorageError("cannot open " + htmlPath);
        out << verity::engine::Exporter::toHtml(report);
        std::cout << "wrote " << htmlPath << " (offline, assets embedded)\n";
    }
    return kExitOk;
}

int cmdSave(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity save <graph.vg>\n";
        return kExitUsage;
    }
    const verity::engine::VerityEngine engine =
        verity::engine::VerityEngine::fromExtractionFile(readSessionExtraction());
    const auto records =
        verity::io::GraphSerializer::buildRecords(engine.store(), engine.graph());
    verity::io::GraphSerializer::save(records, args.positional[0]);
    std::cout << "saved " << records.size() << " records to " << args.positional[0]
              << " (binary, offset-indexed)\n";
    return kExitOk;
}

int cmdLoad(const Args& args) {
    if (args.positional.empty()) {
        std::cerr << "usage: verity load <graph.vg> [--fetch <id>]\n";
        return kExitUsage;
    }
    const std::string& path = args.positional[0];
    std::string fetchId;
    for (std::size_t i = 1; i + 1 < args.positional.size(); i += 2) {
        if (args.positional[i] == "--fetch") fetchId = args.positional[i + 1];
    }
    if (!fetchId.empty()) {
        // RANDOM ACCESS: only the index + one record are read
        const auto record = verity::io::GraphSerializer::fetchRecord(path, fetchId);
        std::cout << "random-access fetch of '" << fetchId << "': kind " << +record.kind
                  << " payload " << record.payload << "\n";
        return kExitOk;
    }
    const auto index = verity::io::GraphSerializer::loadIndex(path);
    const auto records = verity::io::GraphSerializer::load(path);
    std::cout << "loaded " << path << ": format v" << index.version << ", "
              << records.size() << " record(s), index entries " << index.recordCount << "\n";
    std::size_t entities = 0, claims = 0, events = 0, documents = 0, edges = 0;
    for (const auto& r : records) {
        switch (r.kind) {
            case 1: ++entities; break;
            case 2: ++claims; break;
            case 3: ++events; break;
            case 4: ++documents; break;
            case 5: ++edges; break;
            default: break;
        }
    }
    std::cout << "  entities " << entities << " claims " << claims << " events "
              << events << " documents " << documents << " edges " << edges << "\n";
    return kExitOk;
}

int cmdVerifyLedger(const Args& args) {
    if (args.positional.size() != 1) {
        std::cerr << "usage: verity verify-ledger <ledger.json>\n";
        return kExitUsage;
    }
    const auto ledger = verity::engine::Exporter::loadLedger(args.positional[0]);
    long firstBad = -1;
    const bool ok = verity::engine::Exporter::verifyLedgerJson(ledger, firstBad);
    if (ok) {
        std::cout << "ledger VERIFIED (" << ledger.at("entries").get<int>()
                  << " entries, teaching-grade chain, head "
                  << ledger.at("head").get<std::string>() << ")\n";
        return kExitOk;
    }
    std::cout << "ledger TAMPERED: first bad entry index " << firstBad << "\n";
    return kExitDataError;
}

'''
src = src.replace(marker, new_cmds + marker)

src = src.replace(
    '        if (command == "trace") return cmdTrace(args);',
    '        if (command == "trace") return cmdTrace(args);\n'
    '        if (command == "export") return cmdExport(args);\n'
    '        if (command == "save") return cmdSave(args);\n'
    '        if (command == "load") return cmdLoad(args);\n'
    '        if (command == "verify-ledger") return cmdVerifyLedger(args);')

open(p, "w", encoding="utf-8", newline="\n").write(src)
print("main.cpp patched for phase 4")
