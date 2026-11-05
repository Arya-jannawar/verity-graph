# ASSUMPTIONS.md — recorded defaults (SPEC.md unavailable)

The original `SPEC.md` was not present in the workspace. Per the build brief,
PHASES.md (Appendix A) plus the brief's explicitly proposed defaults form the
implementation baseline. Every assumption below is reversible and recorded
here; none silently narrows scope.

| # | Assumption / default | Rationale |
|---|---|---|
| A1 | Fictional companies: **Aster Vale Systems** and **Northstar Meridian Labs**; product **Product X**; persons John Smith, Maria Chen, David Okoro, Elena Vasquez (intermediary); locations Bellhaven, Crescent Bay; meeting venue "Café Meridian" (UTF-8 fixture) | Brief §10 named the two companies; the rest follows PHASES.md planted-case needs |
| A2 | Padded canonical IDs (D001, C001, E001, EV001); unpadded input accepted and canonicalized; UI may render "D6" but storage/CLI/fixtures use padded form | Brief §10; gold files written padded |
| A3 | C++17, CMake ≥3.20 + Ninja, single-header vendored deps (nlohmann/json 3.11.3, doctest 2.4.12, vis-network 9.1.9) | Brief §5 stack guidance; vendor pins recorded in `third_party/THIRD_PARTY.md` |
| A4 | Primary Windows toolchain: LLVM clang 23.1.4 (windows-msvc target). Sanitizer evidence: MSVC `cl.exe /fsanitize=address` (RelWithDebInfo) | clang ASan on windows-msvc cannot unwind C++ exceptions (see LIMITATIONS); MSVC ASan passes the full suite including throw-paths |
| A5 | Phase-1 cross-process CLI: `ingest` stores the extraction path in `out/verity_session.json`; read commands re-load deterministically. Binary `out/graph.vg` replaces this in Phase 4 | Brief §6 requires an explicit session mechanism for the original in-memory ingest example |
| A6 | Predicate `announced`/`launched` may carry free-text (StringValue) objects in phase-1 wire claims; domain/range table notes `"string"` in range | Dataset needs a partnership announcement before Event nodes exist (events arrive Phase 2) |
| A7 | Gold annotations are hand-authored; span offsets were computed once by the checked-in tooling `scripts/make_phase1_gold.py` (occurrence-indexed, ambiguity-safe) and are re-verified by pytest and the C++ loader | Brief §8: ambiguous spans must never be silently first-matched |
| A8 | Report payload schema (root keys) frozen in CONTRACT.md §8; five-tab UI reads `out/graph.json` only | Brief §5, §9 |
| A9 | Severity scale `low|medium|high`, separate from extraction confidence | Brief §8 conflict-semantics item 5 |
| A10 | Mock extractor merges gold fragments and errors loudly on duplicate claim/event ids or conflicting entity identity | Brief §7: gold and extraction data must stay separable and auditable |
