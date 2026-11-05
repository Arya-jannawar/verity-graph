# SPEC_DERIVED.md — derived specification (NOT the original SPEC.md)

**This file is a derived specification.** The original `SPEC.md` referenced by
PHASES.md was unavailable when this build started, so this document states
what was derived from the two supplied documents:

1. **PHASES.md** (the four-person phase plan, reproduced as Appendix A of the
   build brief) — the technical scope per phase: OOP units, DM units, DSA
   structures, AI capabilities, engine/CLI commands, sample data and docs.
2. **The build brief** (`1st_Prompt_VERITY_GRAPH_ZAI_SUPER_PROMPT.txt`) —
   the whole-project request, product behavior, correctness rules, frontend
   direction, dataset requirements, CLI journeys, and test/proof obligations.

Where the two documents describe the same thing, PHASES.md governs technical
scope and the brief governs product behavior and the four-phase-in-one
workflow (its embedded approval pauses are the original team's process, not
this build's). Conflicts were resolved explicitly in `docs/DECISIONS.md`.

Nothing in this file claims to reproduce SPEC.md's exact contents. Where a
SPEC.md detail would have mattered and was unknowable, the choice is recorded
as a default in `docs/ASSUMPTIONS.md` and implemented reversibly.
