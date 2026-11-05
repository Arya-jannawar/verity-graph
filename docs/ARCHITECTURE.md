# ARCHITECTURE.md

```mermaid
flowchart LR
    subgraph Python["ai/ — extraction (Python)"]
        TXT[txt_reader] --> EXT[BaseExtractor]
        MOCK[MockExtractor] --> EXT
        RULE[RuleBasedExtractor] --> EXT
        LLM[LLMExtractor<br>optional, fake-client tested] --> EXT
        EVAL[evaluation P/R] --> EXT
        EXT --> SCHEMA[extraction.schema.json<br>jsonschema validation]
    end

    subgraph CPP["verity engine (C++17)"]
        subgraph io
            JL[io::JsonLoader<br>structural + semantic validation]
            SER[io::GraphSerializer<br>binary + offset index]
        end
        subgraph engine
            GB[GraphBuilder] --> VE[VerityEngine]
            ER[EntityResolver] --> VE
            TB[TimelineBuilder] --> VE
            CD[ConflictDetector<br>Strategy rules + Observer] --> VE
            REAS[Reasoning:<br>EvidenceBuilder / ProvenanceTracer / Ranker] --> VE
            EX[Exporter<br>json / dot / html]
        end
        subgraph ds["ds (handwritten)"]
            L[LinkedList] --> HT[HashTable]
            Q[Queue] --> GA[GraphAlgo BFS/DFS]
            G[Graph] --> ALG[GraphAlgorithms<br>Dijkstra/Kruskal/Prim/Tarjan]
            AVL[AVLTree] --> TIMELINE
            TRIE[Trie] --> RESOLVE
            DSU[DisjointSet] --> KRUSKAL
            HP[Heap] --> DIJKSTRA
            S[SortAlgo / Levenshtein / TopoSort / StringSearch / Trees / GraphMatrix]
        end
        subgraph dm["dm (discrete math)"]
            NT[NumTheory] --> CRYPTO[Crypto MR/CRT/Fermat]
            FS[FiniteSet] --> COUNT[Counting]
            REL[Relations] --> INF[Inference]
            LOGIC[Logic] --> QUANT[Quantifiers]
            NO[NoisyOr] --> RANKER
            GA2[GraphAnalysis] --> CLI2
            LEDGER[Ledger chain]
        end
        subgraph model
            NODE[Node hierarchy<br>Entity/ClaimNode/EventNode/DocumentNode<br>Document family/Evidence/Author diamond]
            STORE[NodeStore<br>Rule of Five]
        end
    end

    CLI["app/main.cpp — `verity` CLI"]
    UI["ui/ — five-tab Evidence Observatory<br>vis-network (vendored, offline)"]

    SCHEMA --> JL --> GB
    JL --> ER --> TB --> CD --> REAS
    VE --> CLI
    CLI --> SER
    CLI --> EX --> REPORT["out/graph.json<br>out/graph.dot<br>out/report.html"]
    REPORT --> UI
    STORE --> NODE
    GB --> STORE
```

## Data flow (one ingest cycle)

1. `ai/run_extract.py` reads `data/sample/*.txt` (or PDF/DOCX via readers),
   normalizes (CRLF→LF, NFC, single trailing newline), runs an extractor,
   validates against `ai/schema/extraction.schema.json` **and** semantic
   checks (code-point span exactness, reference integrity), writes
   `out/extraction.json`.
2. `verity ingest` re-validates EVERYTHING independently in C++
   (`io::JsonLoader` — schema-shaped structural checks + span slicing +
   reference integrity + confidence bounds), builds the typed evidence graph
   (`engine::GraphBuilder` over `ds::Graph<NodeId, EdgeAttr>` with a
   `model::NodeStore` owner), runs `EntityResolver` (Trie blocking →
   similarity policy → dm::Relation equivalence closure ↔ DisjointSet
   cross-check) and `TimelineBuilder` (strict-order check; cycle → finding
   with witness). Session path stored in `out/verity_session.json`.
3. Read commands re-load deterministically; `conflicts` grounds the
   rules.txt strategies through the HashTable claim index; `circular`,
   `keyentities`, `backbone`, `path`, `top-evidence`, `trace` run the
   reasoning engines; `export` writes the complete report payload the UI
   consumes; `save`/`load` persist binary records with an offset index;
   `verify-ledger` recomputes the teaching-grade chain.

## Ownership rules

* `model::NodeStore` owns all `Node` objects (raw `new`/`delete` inside the
  Rule-of-Five class; everything else passes `unique_ptr`).
* `ds`/`dm` are header-only and never own engine state.
* `ConflictListener`s are non-owning observers registered by callers.

## Wording contract

Findings carry the exact phrases "Potential contradiction" /
"Potential temporal contradiction". The UI, exporters and CLI never emit
truth verdicts, lie-detector claims or probabilities of truth. The noisy-OR
aggregate is labeled a confidence heuristic wherever it appears.
