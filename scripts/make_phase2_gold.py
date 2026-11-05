#!/usr/bin/env python
"""Generates the phase-2 gold annotations (D004-D005) with exact code-point
spans — same policy as scripts/make_phase1_gold.py (occurrence-indexed search,
programmatic offsets, self-check). Development tooling, not pipeline code.

Usage: .venv/Scripts/python.exe -I scripts/make_phase2_gold.py
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
        "D004": read_txt(SAMPLE / "d004_consulting_note.txt"),
        "D005": read_txt(SAMPLE / "d005_milestones_memo.txt"),
    }

    # ---------------- D004: alias variants of John Smith -------------------
    t = texts["D004"]
    d004 = {
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D004",
            "filename": "d004_consulting_note.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"title": "Consulting Note", "language": "en"},
            "citations": [],
        }],
        "entities": [
            {"id": "E005", "name": "John Smith", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D004",
                           "span": span_of("D004", t, "John Smith", 0)}]},
            {"id": "E010", "name": "J. Smith", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D004",
                           "span": span_of("D004", t, "J. Smith")}]},
            {"id": "E011", "name": "Mr. Smith", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D004",
                           "span": span_of("D004", t, "Mr. Smith")}]},
            {"id": "E012", "name": "Smith, John", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D004",
                           "span": span_of("D004", t, "Smith, John")}]},
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
             "aliases": [],
             "mentions": [{"document_id": "D004",
                           "span": span_of("D004", t, "Aster Vale Systems")}]},
        ],
        "claims": [],
        "events": [],
        "citations": [],
    }

    # ---------------- D005: temporal cycle in happened_before claims -------
    t = texts["D005"]
    d005 = {
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D005",
            "filename": "d005_milestones_memo.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"title": "Product X Milestones Memo", "language": "en"},
            "citations": [],
        }],
        "entities": [
            {"id": "E004", "name": "Product X", "type": "Product",
             "aliases": [],
             "mentions": [{"document_id": "D005",
                           "span": span_of("D005", t, "Product X", 0)}]},
        ],
        "claims": [
            {"id": "C007", "subject_id": "EV001", "predicate": "happened_before",
             "object": {"kind": "EntityRef", "entity_id": "EV002"},
             "polarity": "Positive", "confidence": 0.9,
             "document_id": "D005",
             "span": span_of("D005", t,
                             "Product X reached internal beta before the pilot rollout"),
             "valid_from": None, "valid_to": None},
            {"id": "C008", "subject_id": "EV002", "predicate": "happened_before",
             "object": {"kind": "EntityRef", "entity_id": "EV003"},
             "polarity": "Positive", "confidence": 0.9,
             "document_id": "D005",
             "span": span_of("D005", t,
                             "The pilot\nrollout happened before the public beta"),
             "valid_from": None, "valid_to": None},
            {"id": "C009", "subject_id": "EV003", "predicate": "happened_before",
             "object": {"kind": "EntityRef", "entity_id": "EV001"},
             "polarity": "Positive", "confidence": 0.9,
             "document_id": "D005",
             "span": span_of("D005", t,
                             "the public beta happened before the internal beta review"),
             "valid_from": None, "valid_to": None},
        ],
        "events": [
            {"id": "EV001", "label": "internal beta", "entity_id": "E004",
             "date": "2024", "document_id": "D005",
             "span": span_of("D005", t, "internal beta", 0)},
            {"id": "EV002", "label": "pilot rollout", "entity_id": "E004",
             "date": "2024", "document_id": "D005",
             "span": span_of("D005", t, "pilot\nrollout")},
            {"id": "EV003", "label": "public beta", "entity_id": "E004",
             "date": "2024", "document_id": "D005",
             "span": span_of("D005", t, "public beta", 0)},
        ],
        "citations": [],
    }

    GOLD.mkdir(parents=True, exist_ok=True)
    for name, fragment in (("d004", d004), ("d005", d005)):
        out = GOLD / f"{name}.gold.json"
        out.write_text(json.dumps(fragment, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        print(f"wrote {out.relative_to(ROOT)}")

    # self-check
    for fragment in (d004, d005):
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
