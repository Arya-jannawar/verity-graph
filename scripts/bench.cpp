// VERITY GRAPH — scripts/bench.cpp: the four required benchmark families
// with REAL measured numbers (never invented):
//   1. list vs matrix graph at 1k/5k/10k nodes
//   2. handwritten HashTable vs std::unordered_map at 100k keys
//   3. merge vs quick vs std::sort
//   4. naive pairwise vs indexed conflict detection (comparison counts)
#include <algorithm>
#include <chrono>
#include <functional>
#include <cstdio>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "verity/ds/Graph.hpp"
#include "verity/ds/GraphMatrix.hpp"
#include "verity/ds/HashTable.hpp"
#include "verity/ds/Heap.hpp"
#include "verity/ds/SortAlgo.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double measureMs(const std::function<void()>& work) {
    const auto start = Clock::now();
    work();
    const auto end = Clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // namespace

int main() {
    std::printf("VERITY benchmarks — real measurements, Release build\n");
    std::printf("hardware note: see docs/BENCHMARKS.md header for the machine\n\n");

    // --- 1. list vs matrix at 1k/5k/10k nodes --------------------------------
    for (const int n : {1000, 5000, 10000}) {
        std::mt19937 rng(n);
        // build edge lists first (deterministic workload)
        std::vector<std::pair<int, int>> edges;
        const int edgeCount = n * 4;   // sparse: ~4 edges per node
        for (int i = 0; i < edgeCount; ++i) {
            edges.emplace_back(static_cast<int>(rng() % n), static_cast<int>(rng() % n));
        }
        double listBuild = 0, matrixBuild = 0, listQuery = 0, matrixQuery = 0;
        {
            verity::ds::Graph<int, double> g(true);
            listBuild = measureMs([&] {
                for (int i = 0; i < n; ++i) g.addNode(i);
                for (const auto& [a, b] : edges) g.addEdge(a, b, 1.0);
            });
            std::size_t hits = 0;
            listQuery = measureMs([&] {
                for (int i = 0; i < n; ++i) {
                    if (g.hasEdge(i, (i + 1) % n)) ++hits;
                }
            });
            (void)hits;
        }
        {
            verity::ds::GraphMatrix<int, double> g(true);
            matrixBuild = measureMs([&] {
                for (int i = 0; i < n; ++i) g.addNode(i);
                for (const auto& [a, b] : edges) g.addEdge(a, b, 1.0);
            });
            std::size_t hits = 0;
            matrixQuery = measureMs([&] {
                for (int i = 0; i < n; ++i) {
                    if (g.hasEdge(i, (i + 1) % n)) ++hits;
                }
            });
            (void)hits;
        }
        std::printf("graph %5d nodes + %6d edges: list build %8.2f ms  matrix build %8.2f ms  "
                    "|  list hasEdge x%d %8.2f ms  matrix hasEdge x%d %8.2f ms\n",
                    n, edgeCount, listBuild, matrixBuild, n, listQuery, n, matrixQuery);
    }

    // --- 2. HashTable vs std::unordered_map at 100k keys ----------------------
    {
        const int k = 100000;
        std::mt19937 rng(k);
        std::vector<std::string> keys;
        for (int i = 0; i < k; ++i) keys.push_back("key_" + std::to_string(rng()));
        double mineInsert = 0, stdInsert = 0, mineLookup = 0, stdLookup = 0;
        {
            verity::ds::HashTable<std::string, int> t;
            mineInsert = measureMs([&] {
                for (int i = 0; i < k; ++i) t.insert(keys[static_cast<std::size_t>(i)], i);
            });
            std::size_t found = 0;
            mineLookup = measureMs([&] {
                for (int i = 0; i < k; ++i) {
                    if (t.contains(keys[static_cast<std::size_t>(i)])) ++found;
                }
            });
            (void)found;
        }
        {
            std::unordered_map<std::string, int> t;
            t.reserve(k);
            stdInsert = measureMs([&] {
                for (int i = 0; i < k; ++i) t.emplace(keys[static_cast<std::size_t>(i)], i);
            });
            std::size_t found = 0;
            stdLookup = measureMs([&] {
                for (int i = 0; i < k; ++i) {
                    if (t.count(keys[static_cast<std::size_t>(i)])) ++found;
                }
            });
            (void)found;
        }
        std::printf("hashtable %d keys: mine insert %8.2f ms  std insert %8.2f ms  "
                    "|  mine lookup %8.2f ms  std lookup %8.2f ms\n",
                    k, mineInsert, stdInsert, mineLookup, stdLookup);
    }

    // --- 3. merge vs quick vs std::sort ---------------------------------------
    {
        const int k = 200000;
        std::mt19937 rng(k);
        std::vector<int> data(k);
        for (int& v : data) v = static_cast<int>(rng());
        double mergeMs = 0, quickMs = 0, stdMs = 0;
        mergeMs = measureMs([&] { (void)verity::ds::mergeSort(data); });
        quickMs = measureMs([&] { (void)verity::ds::quickSort(data); });
        stdMs = measureMs([&] {
            std::vector<int> copy = data;
            std::sort(copy.begin(), copy.end());
        });
        std::printf("sort %d ints: merge %8.2f ms  quick %8.2f ms  std::sort %8.2f ms\n",
                    k, mergeMs, quickMs, stdMs);
    }

    // --- 4. naive pairwise vs indexed conflict detection -----------------------
    {
        const int claimCount = 2000;
        std::mt19937 rng(claimCount);
        // claims: (subject 0..199, predicate 0..2)
        struct Claim { int subject; int predicate; };
        std::vector<Claim> claims(claimCount);
        for (auto& c : claims) {
            c.subject = static_cast<int>(rng() % 200);
            c.predicate = static_cast<int>(rng() % 3);
        }
        long long naiveComparisons = 0;
        double naiveMs = measureMs([&] {
            for (std::size_t i = 0; i < claims.size(); ++i) {
                for (std::size_t j = i + 1; j < claims.size(); ++j) {
                    ++naiveComparisons;
                    if (claims[i].subject == claims[j].subject &&
                        claims[i].predicate == claims[j].predicate) {
                        // potential conflict found (count only)
                    }
                }
            }
        });
        long long indexedComparisons = 0;
        double indexedMs = measureMs([&] {
            verity::ds::HashTable<std::string, std::vector<int>> index;
            for (std::size_t i = 0; i < claims.size(); ++i) {
                const std::string key = std::to_string(claims[i].subject) + "|" +
                                        std::to_string(claims[i].predicate);
                if (auto* bucket = index.get(key)) {
                    for (const int other : *bucket) {
                        ++indexedComparisons;
                        (void)other;
                    }
                    bucket->push_back(static_cast<int>(i));
                } else {
                    index.insert(key, {static_cast<int>(i)});
                }
            }
        });
        std::printf("conflict detection %d claims: naive %10lld comparisons %8.2f ms  "
                    "|  indexed %10lld comparisons %8.2f ms\n",
                    claimCount, naiveComparisons, naiveMs, indexedComparisons, indexedMs);
    }

    return 0;
}
