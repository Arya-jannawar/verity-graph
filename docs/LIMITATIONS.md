# LIMITATIONS.md — stated plainly

## Environment-verified vs environment-blocked

| Check | Status | Note |
|---|---|---|
| C++ suite (clang, Release, zero warnings) | Verified | 182/182 cases, 148,739 assertions |
| C++ suite (MSVC ASan, RelWithDebInfo) | Verified | 182/182, no reports |
| Python pytest | Verified | 39/39 |
| UBSan | Not run | unavailable on the Windows host toolchains; run on Linux via `scripts/build.sh --sanitize --tests` (CMake enables `-fsanitize=undefined` for non-WIN32) |
| valgrind (NodeStore) | Not run | not available on Windows; the equivalent evidence is the ASan run over the same ownership tests |
| Browser screenshots | Not run | no browser automation in this environment; the UI was verified by code review + the offline HTML export including the embedded vis-network bundle. Manual visual checklist below |

## Manual visual checklist (for an examiner with a browser)

1. Open `ui/index.html` via a local static server
   (`python -m http.server --directory ui`) or use the file-import fallback.
2. Graph tab renders the typed graph (4 colors by kind), 2-hop default
   neighborhood with shown/total counts; node list gives canvas-free access.
3. Conflicts tab: every finding says "Potential contradiction" (temporal ones
   "Potential temporal contradiction"), excerpts quoted, benign explanations
   visible, no verdict badges.
4. Timeline tab: cycle witness visible; Hasse withheld for cyclic data.
5. Provenance tab: circular group D006→D007→D008→D006; ledger badge labeled
   teaching-grade with the "loading does not re-verify" note.
6. DM Lab: every card computes live; fixture cross-check lines read "matches".
7. 200% zoom keeps text readable; keyboard focus visible on all controls.

## Honest technical limitations

1. **Negation scope is naive** (60-character window, cue may sit inside the
   match): an unrelated earlier negation leaks into a later claim's polarity.
   Published deliberately by `test_negation_scope_limitation_is_published`.
2. **Rule-extractor recall is low** (P=0.333 R=0.118): templates are
   conservative; unusual phrasings produce no claims rather than fabricated
   ones. The report prints the real numbers.
3. **Noisy-OR is a confidence heuristic** under independence assumptions, not
   a truth probability; circular sources are halved as a conservative penalty.
4. **The ledger is teaching-grade** (modular rotate-mix chain) — it
   demonstrates chain integrity; it is NOT cryptographic (no collision
   resistance against an adversary).
5. **CRT is teaching-scale** (moduli < 2^32 each; combined product must fit
   uint64) — overflow is rejected with an error, never wrapped.
6. **Miller-Rabin uses a fixed 12-witness set** — deterministic over all of
   uint64; the probable-prime framing is kept for honesty.
7. **Hamiltonian brute force is capped at 10 vertices**, power sets at 20
   elements, linear extensions at n=9, truth tables at 12 variables,
   resolution at 32 clauses — every exponential operation is bounded with a
   thrown error, never a freeze.
8. **Dijkstra's weight (1 − confidence)** minimizes that defined additive
   cost; it is not a generic reliability score. The backbone (Kruskal MST over
   the undirected projection) minimizes the same defined cost.
9. **In-memory phase-1 session**: commands re-load the extraction each run;
   the binary `graph.vg` records the graph but cross-process incremental
   state is out of scope.
10. **Windows specifics**: LLVM clang's ASan cannot unwind C++ exceptions on
    windows-msvc (sanitizer evidence therefore uses the MSVC ASan build);
    valgrind and UBSan unavailable; the batch-orientated scripts cover Git
    Bash and PowerShell instead of POSIX-only tooling.
11. **Single run per benchmark** (no variance/warmup statistics) — the
    workload is deterministic and the numbers are honest single measurements.
12. **The UI has no server**: it reads the exported JSON payload (with a
    file-import fallback for file://); it never fabricates backend behavior —
    new PDF/DOCX extraction always goes through the Python/C++ pipeline.
