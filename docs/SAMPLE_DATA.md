# SAMPLE_DATA.md — fictional dataset reference

All data is FICTIONAL and labeled as such everywhere it is displayed.
Companies: **Aster Vale Systems**, **Northstar Meridian Labs**.
Canonical IDs are padded (`D006`); the UI may display `D6` (documented
normalization), storage/CLI/fixtures always use the padded form.

## Document inventory

| ID | File | Content | Planted role |
|---|---|---|---|
| D001 | d001_aster_vale_overview.txt | Aster Vale overview: CEO Maria Chen, HQ Bellhaven, Product X, John Smith | neutral base; C001/C002 |
| D002 | d002_northstar_overview.txt | Northstar overview: CEO David Okoro, Crescent Bay, consultant Elena Vasquez | neutral base; C003/C004 |
| D003 | d003_partnership_wire.txt | neutral partnership announcement (announced, NOT launched); cites D001+D002; "Café Meridian" UTF-8 | benign announced-vs-launch anchor; UTF-8 spans; C005/C006 |
| D004 | d004_consulting_note.txt | alias variants: John Smith / J. Smith / Mr. Smith / Smith, John | alias resolution (planted case) |
| D005 | d005_milestones_memo.txt | happened-before cycle: EV001 internal beta → EV002 pilot → EV003 public beta → EV001 | temporal cycle (planted case) |
| D006 | d006_press_release.txt | launched 2024-03-10 @Bellhaven, announced 2024-03, Elena present, 5 modules; cites D007 | date conflict side A; benign month-vs-day anchor |
| D007 | d007_industry_wire.txt | launched 2024-06-05 @Crescent Bay, CEO Priya Raman since 2023-06, 8 modules; cites D008 | date conflict side B; CEO conflict; numeric conflict |
| D008 | d008_analyst_note.txt | did NOT launch on 2024-06-05 (negative polarity); cites D006 | negation conflict; closes the citation cycle |
| D009 | d009_board_memo.txt | Elena present at Crescent Bay summit 2024-03-10; Maria Chen CEO since 2022-01; cites D002 | alibi conflict; CEO conflict side B; intermediary anchor |
| D010 | d010_final_summary.txt | joint summary; cites D006+D007 | citation hub |
| d004_format_variant.pdf | PDF fixture | FORMAT VARIANT of overview content (no new semantic claims) | pdf reader test |
| d005_format_variant.docx | DOCX fixture | FORMAT VARIANT (no new semantic claims) | docx reader test |

Format-variant rule: d004_format_variant.pdf / d005_format_variant.docx are
format fixtures ONLY — semantic originals are d001–d010 (txt); duplicate claim
counting across variants is impossible because variants carry no claims.

## Planted cases → expected CLI output

| Case | Expected behavior | Where to see it |
|---|---|---|
| 4 alias variants resolve | canonical E005 with aliases J. Smith, John Smith, Mr. Smith, Smith, John | `verity ingest` "entity resolution" line |
| unrelated same-surname people stay separate | E020 Will Smith / E021 A. Smith (alias Alice Smith) never merge | tests/engine/test_engine2.cpp |
| temporal cycle | "Potential temporal contradiction" + witness EV001→EV002→EV003→EV001, Hasse WITHHELD | `verity timeline` |
| date conflict | C010 vs C014 (2024-03-10 vs 2024-06-05) | `verity conflicts` X002 |
| CEO conflict | C002/C015/C020 overlapping has_ceo clashes | `verity conflicts` X001, X004 |
| alibi conflict | C012 vs C019 same-day two places | `verity conflicts` X005 |
| negation conflict | C014 (Positive June 5) vs C017 (Negative June 5) | `verity conflicts` X006 |
| numeric conflict | C013 (5 modules) vs C016 (8 modules) | `verity conflicts` X007 |
| circular sourcing | D006→D007→D008→D006 witness | `verity circular` |
| benign announced-vs-launch | NO finding touches C011 | conflicts output + test |
| benign month-vs-day | NO finding for C011 (2024-03) vs C021 (2024-03-15) | conflicts output + test |
| intermediary disconnects | removing E009 splits the D006-D009 co-mention projection | tests/engine/test_conflicts.cpp |

## Gold annotations

`data/gold/d00N.gold.json` fragments are hand-authored; span offsets were
computed by the checked-in tooling (`scripts/make_phase*_gold.py`,
occurrence-indexed — ambiguous quotes are NEVER first-match-assigned) and are
re-verified by pytest AND the C++ loader on every ingest.
