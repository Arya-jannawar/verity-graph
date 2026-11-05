// VERITY GRAPH — tests/io/test_phase4.cpp
// Phase 4: binary serializer (round-trip, random access, corruption),
// Miller-Rabin/CRT/Fermat, evidence ledger verify+tamper, tree checks,
// expression tree, adjacency-matrix variant, spanning-tree verification.

#include "doctest/doctest.h"

#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "verity/dm/Crypto.hpp"
#include "verity/dm/Ledger.hpp"
#include "verity/ds/GraphMatrix.hpp"
#include "verity/ds/GraphAlgorithms.hpp"
#include "verity/ds/Trees.hpp"
#include "verity/engine/Exporter.hpp"
#include "verity/engine/VerityEngine.hpp"
#include "verity/io/GraphSerializer.hpp"
#include "verity/io/JsonLoader.hpp"
#include "verity/util/VerityError.hpp"

using namespace verity;

namespace {

/// Builds the engine from the merged gold fragments (repo data).
engine::VerityEngine buildEngine() {
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
    std::map<std::string, nlohmann::json> entityById;
    std::vector<std::string> entityOrder;
    for (const char* file : files) {
        std::ifstream in(file);
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
                for (const auto& a : e.at("aliases")) entityById[id]["aliases"].push_back(a);
                for (const auto& m : e.at("mentions")) entityById[id]["mentions"].push_back(m);
            }
        }
    }
    for (const std::string& id : entityOrder) merged["entities"].push_back(entityById[id]);
    engine::VerityEngine engine;
    engine.ingest(io::JsonLoader::parseExtraction(merged.dump()));
    return engine;
}

void corruptByte(const std::string& path, std::size_t offset) {
    std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
    file.seekg(static_cast<std::streamoff>(offset));
    char byte = '\0';
    file.read(&byte, 1);
    byte ^= 0xFF;
    file.seekp(static_cast<std::streamoff>(offset));
    file.write(&byte, 1);
}

} // namespace

TEST_CASE("binary serializer: round-trip equality") {
    const auto engine = buildEngine();
    const auto records =
        io::GraphSerializer::buildRecords(engine.store(), engine.graph());
    CHECK(records.size() > 20u);
    const std::string path = "out/test_graph.vg";
    io::GraphSerializer::save(records, path);

    const auto loaded = io::GraphSerializer::load(path);
    REQUIRE_EQ(loaded.size(), records.size());
    // set-equality on (kind, id, payload): no content lost
    std::set<std::string> before, after;
    for (const auto& r : records) {
        before.insert(std::to_string(r.kind) + "|" + r.id + "|" + r.payload);
    }
    for (const auto& r : loaded) {
        after.insert(std::to_string(r.kind) + "|" + r.id + "|" + r.payload);
    }
    CHECK(before == after);
    std::remove(path.c_str());
}

TEST_CASE("binary serializer: indexed random access without full load") {
    const auto engine = buildEngine();
    const auto records =
        io::GraphSerializer::buildRecords(engine.store(), engine.graph());
    const std::string path = "out/test_random.vg";
    io::GraphSerializer::save(records, path);

    // pick a known claim record
    std::string claimId;
    for (const auto& r : records) {
        if (r.kind == 2) { claimId = r.id; break; }
    }
    REQUIRE_FALSE(claimId.empty());
    const auto fetched = io::GraphSerializer::fetchRecord(path, claimId);
    CHECK_EQ(fetched.kind, 2);
    CHECK_EQ(fetched.id, claimId);
    // payload matches the full-load copy of the same record
    const auto loaded = io::GraphSerializer::load(path);
    for (const auto& r : loaded) {
        if (r.id == claimId) {
            CHECK_EQ(fetched.payload, r.payload);
        }
    }
    CHECK_THROWS_AS((void)io::GraphSerializer::fetchRecord(path, "Z999"),
                    verity::util::StorageError);
    std::remove(path.c_str());
}

TEST_CASE("binary serializer: corruption is rejected (bad magic, bad index, tampered record)") {
    const auto engine = buildEngine();
    const auto records =
        io::GraphSerializer::buildRecords(engine.store(), engine.graph());
    const std::string path = "out/test_corrupt.vg";
    io::GraphSerializer::save(records, path);

    SUBCASE("bad magic") {
        corruptByte(path, 0);
        CHECK_THROWS_AS((void)io::GraphSerializer::load(path), verity::util::StorageError);
    }
    SUBCASE("tampered record byte") {
        corruptByte(path, 64);   // inside the record section
        // load either throws OR round-trips to different content; it must
        // never silently pass
        bool threw = false;
        std::set<std::string> after;
        try {
            for (const auto& r : io::GraphSerializer::load(path)) {
                after.insert(r.id + r.payload);
            }
        } catch (const verity::util::StorageError&) {
            threw = true;
        }
        const bool silentPass = !threw && after.size() == records.size();
        CHECK_FALSE(silentPass);
    }
    SUBCASE("truncated file") {
        std::ifstream in(path, std::ios::binary);
        const std::string content((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
        in.close();
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(content.data(), static_cast<std::streamsize>(content.size() / 2));
        out.close();
        CHECK_THROWS_AS((void)io::GraphSerializer::load(path), verity::util::StorageError);
        CHECK_THROWS_AS((void)io::GraphSerializer::loadIndex(path),
                        verity::util::StorageError);
    }
    std::remove(path.c_str());
}

TEST_CASE("Miller-Rabin: deterministic witness set over uint64") {
    using verity::dm::isProbablePrime;
    // agreement with trial division over a dense range
    for (std::uint64_t n = 0; n < 3000; ++n) {
        CHECK_EQ(isProbablePrime(n), verity::dm::isPrime(n));
    }
    // known large primes and composites
    CHECK(isProbablePrime(2147483647ULL));           // 2^31 - 1
    CHECK_FALSE(isProbablePrime(2147483649ULL));
    CHECK(isProbablePrime(67280421310721ULL));       // Fermat F6 factor
    CHECK(isProbablePrime(9223372036854775783ULL));  // largest prime < 2^63
    CHECK_FALSE(isProbablePrime(9223372036854775807ULL));  // 2^63 - 1 composite
    CHECK_EQ(verity::dm::millerRabinWitnessCount(), 12u);
}

TEST_CASE("CRT: consistent, non-coprime and overflow behavior") {
    using verity::dm::crt;
    const auto solution = crt({2, 3, 2}, {3, 5, 7});
    CHECK_EQ(solution.modulus, 105u);
    CHECK_EQ(solution.x % 3, 2u);
    CHECK_EQ(solution.x % 5, 3u);
    CHECK_EQ(solution.x % 7, 2u);

    // non-coprime moduli must throw with an honest error
    CHECK_THROWS_AS((void)crt({1, 2}, {4, 6}), verity::util::ParseError);
    // inconsistent repeated modulus
    CHECK_THROWS_AS((void)crt({1, 2}, {5, 5}), verity::util::ParseError);
    CHECK_THROWS_AS((void)crt({1}, {0}), verity::util::ParseError);
}

TEST_CASE("Fermat check: primes pass, composites may fail") {
    using verity::dm::fermatCheck;
    CHECK(fermatCheck(2, 97));
    CHECK(fermatCheck(5, 97));
    CHECK_FALSE(fermatCheck(2, 561));   // Carmichael 561 passes some bases; 2 fails
    CHECK_FALSE(fermatCheck(3, 15));
}

TEST_CASE("evidence ledger: verify passes clean, fails on a tampered copy") {
    auto engine = buildEngine();
    const auto ledgerJson = engine::Exporter::buildLedgerJson(engine);
    const std::string path = "out/test_ledger.json";
    engine::Exporter::saveLedger(ledgerJson, path);

    long firstBad = -1;
    CHECK(engine::Exporter::verifyLedgerJson(
        engine::Exporter::loadLedger(path), firstBad));
    CHECK_EQ(firstBad, -1);

    // TAMPER a disposable COPY (pristine file untouched): flip one record
    nlohmann::json tampered = engine::Exporter::loadLedger(path);
    REQUIRE(tampered["entries_data"].size() > 2u);
    std::string original = tampered["entries_data"][1].get<std::string>();
    tampered["entries_data"][1] = original + "|TAMPERED";
    const std::string tamperedPath = "out/test_ledger_tampered.json";
    engine::Exporter::saveLedger(tampered, tamperedPath);

    long tamperedBad = -1;
    CHECK_FALSE(engine::Exporter::verifyLedgerJson(
        engine::Exporter::loadLedger(tamperedPath), tamperedBad));
    CHECK_GE(tamperedBad, 1);   // the tampered entry or later breaks the chain

    // pristine file still verifies
    long pristineBad = -1;
    CHECK(engine::Exporter::verifyLedgerJson(
        engine::Exporter::loadLedger(path), pristineBad));
    std::remove(path.c_str());
    std::remove(tamperedPath.c_str());
}

TEST_CASE("tree law: spanning tree verification and full-binary leaf law") {
    // a 4-cycle + chord: Kruskal result must be a spanning tree
    ds::Graph<std::string, double> g(false);
    for (const char* n : {"A", "B", "C", "D", "E"}) g.addNode(n);
    g.addEdgeUndirected("A", "B", 1);
    g.addEdgeUndirected("B", "C", 2);
    g.addEdgeUndirected("C", "D", 1);
    g.addEdgeUndirected("D", "E", 2);
    g.addEdgeUndirected("A", "E", 4);
    g.addEdgeUndirected("B", "D", 3);
    const auto kruskal = ds::kruskalMst(g, [](const double& w) { return w; });
    std::vector<std::pair<std::string, std::string>> edges;
    for (const auto& [a, b, w] : kruskal) {
        (void)w;
        edges.emplace_back(a, b);
    }
    CHECK(ds::isSpanningTree(g, edges));
    CHECK_EQ(edges.size(), 4u);   // n - 1
    CHECK(ds::isTree(g) == false);   // the FULL graph has a cycle: not a tree

    // the MST edge SUBGRAPH is a tree: rebuild it and check |E| == |V| - 1
    ds::Graph<std::string, double> mst(false);
    for (const char* n : {"A", "B", "C", "D", "E"}) mst.addNode(n);
    for (const auto& [a, b] : edges) mst.addEdgeUndirected(a, b, 1);
    CHECK(ds::isTree(mst));

    // wrong candidate: duplicate edge fails the |E| = |V|-1 law
    edges.push_back(edges.front());
    CHECK_FALSE(ds::isSpanningTree(g, edges));
    CHECK(ds::fullBinaryLeafLaw(3, 4));
    CHECK_FALSE(ds::fullBinaryLeafLaw(3, 5));
}

TEST_CASE("arithmetic expression tree: builds, walks, evaluates") {
    // (2 + 3) * 4 in postfix
    const auto tree = ds::ExpressionTree::fromPostfix({"2", "3", "+", "4", "*"});
    CHECK_EQ(tree.infix(), std::string("((2 + 3) * 4)"));
    CHECK_EQ(tree.prefix(), std::string("* + 2 3 4"));
    CHECK_EQ(tree.postfix(), std::string("2 3 + 4 *"));
    CHECK_EQ(tree.evaluate(), 20LL);

    const auto [internal, leaves] = tree.internalAndLeaves();
    CHECK_EQ(internal, 2u);
    CHECK_EQ(leaves, 3u);
    CHECK(ds::fullBinaryLeafLaw(internal, leaves));

    CHECK_THROWS_AS((void)ds::ExpressionTree::fromPostfix({"2", "+"}),
                    verity::util::ParseError);
    CHECK_THROWS_AS((void)ds::ExpressionTree::fromPostfix({"2", "3", "4"}),
                    verity::util::ParseError);
    const auto div = ds::ExpressionTree::fromPostfix({"5", "0", "/"});
    CHECK_THROWS_AS((void)div.evaluate(), verity::util::ParseError);
}

TEST_CASE("adjacency-matrix variant: same interface, same answers") {
    ds::Graph<std::string, double> list(false);
    ds::GraphMatrix<std::string, double> matrix(false);
    for (const char* n : {"A", "B", "C", "D"}) {
        list.addNode(n);
        matrix.addNode(n);
    }
    const std::vector<std::tuple<const char*, const char*, double>> edges{
        {"A", "B", 1}, {"B", "C", 2}, {"C", "D", 3}, {"D", "A", 4}, {"B", "D", 5}};
    for (const auto& [a, b, w] : edges) {
        list.addEdgeUndirected(a, b, w);
        matrix.addEdgeUndirected(a, b, w);
    }
    CHECK_EQ(list.nodeCount(), matrix.nodeCount());
    CHECK_EQ(list.edgeCount(), matrix.edgeCount());
    for (const auto& a : list.nodes()) {
        for (const auto& b : list.nodes()) {
            CHECK_EQ(list.hasEdge(a, b), matrix.hasEdge(a, b));
            if (a != b) {
                CHECK_EQ(list.degree(a) >= 0, matrix.degree(a) >= 0);
            }
        }
        CHECK_EQ(list.degree(a), matrix.degree(a));
    }
    // degree conventions: loop counts 2 in both variants
    list.addEdgeUndirected("A", "A", 1);
    matrix.addEdgeUndirected("A", "A", 1);
    CHECK_EQ(list.degree("A"), matrix.degree("A"));
    CHECK_EQ(list.degree("A"), 4u);

    CHECK_THROWS_AS((void)matrix.neighbors("ghost"), verity::util::VerityError);
    CHECK_THROWS_AS(matrix.addNode("A"), verity::util::VerityError);

    // transpose matches
    const auto lt = list.transpose();
    const auto mt = matrix.transpose();
    CHECK_EQ(lt.hasEdge("B", "A"), mt.hasEdge("B", "A"));
    CHECK_EQ(lt.edgeCount(), mt.edgeCount());
}

TEST_CASE("exporters: report payload, DOT, standalone HTML") {
    const auto engine = buildEngine();
    const auto report = engine::Exporter::buildReport(engine);
    CHECK_EQ(report["schema_version"], "1.0.0");
    CHECK(report["nodes"].size() > 20u);
    CHECK(report["conflicts"].size() >= 5u);
    // wording guarantee inside the exported payload
    bool wordingOk = true;
    for (const auto& c : report["conflicts"]) {
        const std::string statement = c["statement"].get<std::string>();
        if (statement.find("Potential contradiction") == std::string::npos &&
            statement.find("Potential temporal contradiction") == std::string::npos) {
            wordingOk = false;
        }
    }
    CHECK(wordingOk);
    // ledger section is labeled teaching-grade
    CHECK(std::string(report["ledger"]["mode"].get<std::string>()).find("NOT cryptographic") !=
          std::string::npos);

    const auto dot = engine::Exporter::toDot(engine);
    CHECK(dot.find("digraph verity") != std::string::npos);
    CHECK(dot.find("C010") != std::string::npos);

    const auto html = engine::Exporter::toHtml(report);
    CHECK(html.find("vis") != std::string::npos);
    CHECK(html.find("Fictional demo dataset") == std::string::npos);  // label lives in JSON
    CHECK(html.find("Potential contradiction") != std::string::npos);
}
