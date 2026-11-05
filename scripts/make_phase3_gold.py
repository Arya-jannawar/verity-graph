#!/usr/bin/env python
"""Generates the phase-3 gold annotations (D006-D009) — planted conflicts,
circular sourcing, intermediary, benign near-conflicts — with exact
code-point spans (same occurrence-indexed policy as phases 1-2).

Usage: .venv/Scripts/python.exe -I scripts/make_phase3_gold.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "ai"))

from readers.txt_reader import read_txt  # noqa: E402

SAMPLE = ROOT / "data" / "sample"
GOLD = ROOT / "data" / "gold"


def span_of(doc_id: str, text: str, quote: str, occurrence: int = 0) -> dict:
    start = -1
    for _ in range(occurrence + 1):
        start = text.find(quote, start + 1)
        if start < 0:
            raise SystemExit(f"quote {occurrence=} not found in {doc_id}: {quote!r}")
    return {"start": start, "end": start + len(quote), "quote": quote}


def main() -> int:
    texts = {
        f"D{n:03d}": read_txt(SAMPLE / name)
        for n, name in (
            (6, "d006_press_release.txt"),
            (7, "d007_industry_wire.txt"),
            (8, "d008_analyst_note.txt"),
            (9, "d009_board_memo.txt"),
        )
    }

    def doc_record(doc_id: str, filename: str, citations: list[str]) -> dict:
        return {
            "id": doc_id,
            "filename": filename,
            "format": "txt",
            "normalized_text": texts[doc_id],
            "metadata": {"language": "en"},
            "citations": citations,
        }

    fragments = []

    # ---------------- D006: press release (March launch, benign announced) --
    t = texts["D006"]
    fragments.append({
        "schema_version": "1.0.0",
        "documents": [doc_record("D006", "d006_press_release.txt", ["D007"])],
        "entities": [
            {"id": "E004", "name": "Product X", "type": "Product", "aliases": [],
             "mentions": [{"document_id": "D006",
                           "span": span_of("D006", t, "Product X", 0)}]},
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization", "aliases": [],
             "mentions": [{"document_id": "D006",
                           "span": span_of("D006", t, "Aster Vale Systems", 0)}]},
            {"id": "E003", "name": "Bellhaven", "type": "Location", "aliases": [],
             "mentions": [{"document_id": "D006",
                           "span": span_of("D006", t, "Bellhaven", 0)}]},
            {"id": "E009", "name": "Elena Vasquez", "type": "Person", "aliases": [],
             "mentions": [{"document_id": "D006",
                           "span": span_of("D006", t, "Elena Vasquez")}]},
        ],
        "claims": [
            {"id": "C010", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "DateValue", "date": "2024-03-10"},
             "polarity": "Positive", "confidence": 0.9, "document_id": "D006",
             "span": span_of("D006", t, "Product X launched in Bellhaven on 2024-03-10"),
             "valid_from": None, "valid_to": None},
            {"id": "C011", "subject_id": "E001", "predicate": "announced",
             "object": {"kind": "DateValue", "date": "2024-03"},
             "polarity": "Positive", "confidence": 0.85, "document_id": "D006",
             "span": span_of("D006", t, "Aster Vale Systems had announced Product X in 2024-03"),
             "valid_from": None, "valid_to": None},
            {"id": "C012", "subject_id": "E009", "predicate": "present_at",
             "object": {"kind": "StringValue", "text": "Bellhaven showcase"},
             "polarity": "Positive", "confidence": 0.85, "document_id": "D006",
             "span": span_of("D006", t, "Elena Vasquez was present at the showcase in Bellhaven"),
             "valid_from": "2024-03-10", "valid_to": "2024-03-10"},
            {"id": "C013", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "QuantityValue", "quantity": 5.0, "unit": "modules"},
             "polarity": "Positive", "confidence": 0.8, "document_id": "D006",
             "span": span_of("D006", t, "The company\nshipped 5 modules of Product X at launch"),
             "valid_from": None, "valid_to": None},
        ],
        "events": [
            {"id": "EV004", "label": "Product X Bellhaven launch", "entity_id": "E004",
             "date": "2024-03-10", "document_id": "D006",
             "span": span_of("D006", t, "2024-03-10", 0)},
        ],
        "citations": [{"citing": "D006", "cited": "D007", "span": None}],
    })

    # ---------------- D007: wire (June launch, CEO conflict, numeric) --------
    t = texts["D007"]
    fragments.append({
        "schema_version": "1.0.0",
        "documents": [doc_record("D007", "d007_industry_wire.txt", ["D008"])],
        "entities": [
            {"id": "E004", "name": "Product X", "type": "Product", "aliases": [],
             "mentions": [{"document_id": "D007",
                           "span": span_of("D007", t, "Product X", 0)}]},
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization", "aliases": [],
             "mentions": [{"document_id": "D007",
                           "span": span_of("D007", t, "Aster Vale Systems", 0)}]},
            {"id": "E013", "name": "Priya Raman", "type": "Person", "aliases": [],
             "mentions": [{"document_id": "D007",
                           "span": span_of("D007", t, "Priya Raman")}]},
        ],
        "claims": [
            {"id": "C014", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "DateValue", "date": "2024-06-05"},
             "polarity": "Positive", "confidence": 0.85, "document_id": "D007",
             "span": span_of("D007", t,
                             "Aster Vale Systems launched Product X in Crescent Bay on 2024-06-05"),
             "valid_from": None, "valid_to": None},
            {"id": "C015", "subject_id": "E001", "predicate": "has_ceo",
             "object": {"kind": "EntityRef", "entity_id": "E013"},
             "polarity": "Positive", "confidence": 0.85, "document_id": "D007",
             "span": span_of("D007", t,
                             "Aster Vale\nSystems appointed Priya Raman as chief executive officer"),
             "valid_from": "2023-06", "valid_to": None},
            {"id": "C016", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "QuantityValue", "quantity": 8.0, "unit": "modules"},
             "polarity": "Positive", "confidence": 0.8, "document_id": "D007",
             "span": span_of("D007", t, "Product X shipped 8 modules according to the wire"),
             "valid_from": None, "valid_to": None},
        ],
        "events": [
            {"id": "EV005", "label": "Product X Crescent Bay launch", "entity_id": "E004",
             "date": "2024-06-05", "document_id": "D007",
             "span": span_of("D007", t, "2024-06-05", 0)},
        ],
        "citations": [{"citing": "D007", "cited": "D008", "span": None}],
    })

    # ---------------- D008: analyst note (negation, circular cite) ----------
    t = texts["D008"]
    fragments.append({
        "schema_version": "1.0.0",
        "documents": [doc_record("D008", "d008_analyst_note.txt", ["D006"])],
        "entities": [
            {"id": "E004", "name": "Product X", "type": "Product", "aliases": [],
             "mentions": [{"document_id": "D008",
                           "span": span_of("D008", t, "Product X", 0)}]},
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization", "aliases": [],
             "mentions": [{"document_id": "D008",
                           "span": span_of("D008", t, "Aster Vale Systems", 0)}]},
        ],
        "claims": [
            {"id": "C017", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "DateValue", "date": "2024-06-05"},
             "polarity": "Negative", "confidence": 0.85, "document_id": "D008",
             "span": span_of("D008", t,
                             "Aster Vale Systems did not launch Product X in\nCrescent Bay on 2024-06-05"),
             "valid_from": None, "valid_to": None},
            {"id": "C018", "subject_id": "E004", "predicate": "launched",
             "object": {"kind": "DateValue", "date": "2024-03-10"},
             "polarity": "Positive", "confidence": 0.8, "document_id": "D008",
             "span": span_of("D008", t, "Product\nX had launched in Bellhaven on 2024-03-10"),
             "valid_from": None, "valid_to": None},
        ],
        "events": [],
        "citations": [{"citing": "D008", "cited": "D006", "span": None}],
    })

    # ---------------- D009: board memo (alibi, CEO anchor, intermediary) ----
    t = texts["D009"]
    fragments.append({
        "schema_version": "1.0.0",
        "documents": [doc_record("D009", "d009_board_memo.txt", ["D002"])],
        "entities": [
            {"id": "E009", "name": "Elena Vasquez", "type": "Person", "aliases": [],
             "mentions": [{"document_id": "D009",
                           "span": span_of("D009", t, "Elena Vasquez")}]},
            {"id": "E002", "name": "Maria Chen", "type": "Person", "aliases": [],
             "mentions": [{"document_id": "D009",
                           "span": span_of("D009", t, "Maria Chen")}]},
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
             "aliases": [], "mentions": []},
            {"id": "E006", "name": "Northstar Meridian Labs", "type": "Organization",
             "aliases": [],
             "mentions": [{"document_id": "D009",
                           "span": span_of("D009", t, "Northstar Meridian Labs")}]},
        ],
        "claims": [
            {"id": "C019", "subject_id": "E009", "predicate": "present_at",
             "object": {"kind": "StringValue", "text": "Crescent Bay summit"},
             "polarity": "Positive", "confidence": 0.9, "document_id": "D009",
             "span": span_of("D009", t,
                             "Elena Vasquez was present at the Crescent Bay summit on 2024-03-10"),
             "valid_from": "2024-03-10", "valid_to": "2024-03-10"},
            {"id": "C020", "subject_id": "E001", "predicate": "has_ceo",
             "object": {"kind": "EntityRef", "entity_id": "E002"},
             "polarity": "Positive", "confidence": 0.95, "document_id": "D009",
             "span": span_of("D009", t,
                             "Maria Chen has served as chief\nexecutive officer of Aster Vale Systems"),
             "valid_from": "2022-01", "valid_to": None},
        ],
        "events": [],
        "citations": [{"citing": "D009", "cited": "D002", "span": None}],
    })

    # ---------------- D010: final summary (citation hub, no new claims) ----
    t = texts.get("D010")
    if t is None:
        texts["D010"] = None
    import sys as _sys
    _sys.path.insert(0, str(ROOT / "ai"))
    from readers.txt_reader import read_txt as _read
    t = _read(SAMPLE / "d010_final_summary.txt")
    fragments.append({
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D010",
            "filename": "d010_final_summary.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"language": "en"},
            "citations": ["D006", "D007"],
        }],
        "entities": [],
        "claims": [],
        "events": [],
        "citations": [
            {"citing": "D010", "cited": "D006", "span": None},
            {"citing": "D010", "cited": "D007", "span": None},
        ],
    })


    GOLD.mkdir(parents=True, exist_ok=True)
    names = ["d006", "d007", "d008", "d009", "d010"]
    for name, fragment in zip(names, fragments):
        out = GOLD / f"{name}.gold.json"
        out.write_text(json.dumps(fragment, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        print(f"wrote {out.relative_to(ROOT)}")

    # self-check: every span slices exactly
    for fragment in fragments:
        doc_text = {d["id"]: d["normalized_text"] for d in fragment["documents"]}
        for entity in fragment["entities"]:
            for m in entity["mentions"]:
                assert doc_text[m["document_id"]][m["span"]["start"]:m["span"]["end"]] == m["span"]["quote"]
        for claim in fragment["claims"]:
            text = doc_text[claim["document_id"]]
            assert text[claim["span"]["start"]:claim["span"]["end"]] == claim["span"]["quote"]
        for event in fragment["events"]:
            text = doc_text[event["document_id"]]
            assert text[event["span"]["start"]:event["span"]["end"]] == event["span"]["quote"]
    print("self-check ok: all spans slice exactly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
