#!/usr/bin/env python
"""Generates the phase-1 gold annotations (D001-D003) with exact code-point
spans.

This is DEVELOPMENT TOOLING, not part of the shipped pipeline: it authored the
static files under data/gold/ once, with every span offset computed
programmatically (occurrence-indexed search over the normalized text) instead
of hand-counted. The shipped pytest suite re-verifies every shipped span.

Usage: .venv/Scripts/python.exe -I scripts/make_phase1_gold.py
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
    """Locates the n-th occurrence of `quote` and returns its exact span.

    Ambiguity policy: repeated quotes are resolved by explicit occurrence
    index; the caller must know which occurrence is intended. No silent
    first-match assignment.
    """
    start = -1
    for _ in range(occurrence + 1):
        start = text.find(quote, start + 1)
        if start < 0:
            raise SystemExit(f"quote {occurrence=} not found in {doc_id}: {quote!r}")
    return {"start": start, "end": start + len(quote), "quote": quote}


def main() -> int:
    texts = {
        "D001": read_txt(SAMPLE / "d001_aster_vale_overview.txt"),
        "D002": read_txt(SAMPLE / "d002_northstar_overview.txt"),
        "D003": read_txt(SAMPLE / "d003_partnership_wire.txt"),
    }

    # ---------------- D001: Aster Vale Systems overview (neutral) ----------
    t = texts["D001"]
    d001 = {
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D001",
            "filename": "d001_aster_vale_overview.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"title": "Aster Vale Systems Overview", "language": "en"},
            "citations": [],
        }],
        "entities": [
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
             "aliases": [],
             "mentions": [
                 {"document_id": "D001",
                  "span": span_of("D001", t, "Aster Vale Systems", 0)},
                 {"document_id": "D001",
                  "span": span_of("D001", t, "Aster Vale Systems", 1)},
             ]},
            {"id": "E002", "name": "Maria Chen", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D001", "span": span_of("D001", t, "Maria Chen")}]},
            {"id": "E003", "name": "Bellhaven", "type": "Location",
             "aliases": [],
             "mentions": [{"document_id": "D001", "span": span_of("D001", t, "Bellhaven")}]},
            {"id": "E004", "name": "Product X", "type": "Product",
             "aliases": [],
             "mentions": [{"document_id": "D001", "span": span_of("D001", t, "Product X")}]},
            {"id": "E005", "name": "John Smith", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D001", "span": span_of("D001", t, "John Smith")}]},
        ],
        "claims": [
            {"id": "C001", "subject_id": "E001", "predicate": "located_at",
             "object": {"kind": "EntityRef", "entity_id": "E003"},
             "polarity": "Positive", "confidence": 0.95,
             "document_id": "D001",
             "span": span_of("D001", t,
                             "Aster Vale Systems is a fictional software company based in Bellhaven."),
             "valid_from": None, "valid_to": None},
            {"id": "C002", "subject_id": "E001", "predicate": "has_ceo",
             "object": {"kind": "EntityRef", "entity_id": "E002"},
             "polarity": "Positive", "confidence": 0.95,
             "document_id": "D001",
             "span": span_of("D001", t,
                             "Maria Chen has served as chief executive officer of Aster Vale Systems"),
             "valid_from": "2022-01", "valid_to": None},
        ],
        "events": [],
        "citations": [],
    }

    # ---------------- D002: Northstar Meridian Labs overview (neutral) -----
    t = texts["D002"]
    d002 = {
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D002",
            "filename": "d002_northstar_overview.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"title": "Northstar Meridian Labs Overview", "language": "en"},
            "citations": [],
        }],
        "entities": [
            {"id": "E006", "name": "Northstar Meridian Labs", "type": "Organization",
             "aliases": [],
             "mentions": [
                 {"document_id": "D002",
                  "span": span_of("D002", t, "Northstar Meridian Labs", 0)},
                 {"document_id": "D002",
                  "span": span_of("D002", t, "Northstar Meridian Labs", 1)},
             ]},
            {"id": "E007", "name": "David Okoro", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D002", "span": span_of("D002", t, "David Okoro")}]},
            {"id": "E008", "name": "Crescent Bay", "type": "Location",
             "aliases": [],
             "mentions": [{"document_id": "D002", "span": span_of("D002", t, "Crescent Bay")}]},
            {"id": "E009", "name": "Elena Vasquez", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D002", "span": span_of("D002", t, "Elena Vasquez")}]},
        ],
        "claims": [
            {"id": "C003", "subject_id": "E006", "predicate": "located_at",
             "object": {"kind": "EntityRef", "entity_id": "E008"},
             "polarity": "Positive", "confidence": 0.95,
             "document_id": "D002",
             "span": span_of("D002", t,
                             "Northstar Meridian Labs is a fictional research laboratory located in"),
             "valid_from": None, "valid_to": None},
            {"id": "C004", "subject_id": "E006", "predicate": "has_ceo",
             "object": {"kind": "EntityRef", "entity_id": "E007"},
             "polarity": "Positive", "confidence": 0.95,
             "document_id": "D002",
             "span": span_of("D002", t,
                             "David Okoro is the chief executive officer of Northstar\nMeridian Labs"),
             "valid_from": "2021-06", "valid_to": None},
        ],
        "events": [],
        "citations": [],
    }

    # ---------------- D003: partnership wire (neutral announcement) --------
    t = texts["D003"]
    d003 = {
        "schema_version": "1.0.0",
        "documents": [{
            "id": "D003",
            "filename": "d003_partnership_wire.txt",
            "format": "txt",
            "normalized_text": t,
            "metadata": {"title": "Partnership Wire", "language": "en"},
            "citations": ["D001", "D002"],
        }],
        "entities": [
            {"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
             "aliases": [],
             "mentions": [{"document_id": "D003",
                           "span": span_of("D003", t, "Aster Vale Systems", 0)}]},
            {"id": "E005", "name": "John Smith", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D003",
                           "span": span_of("D003", t, "John Smith")}]},
            {"id": "E006", "name": "Northstar Meridian Labs", "type": "Organization",
             "aliases": [],
             "mentions": [{"document_id": "D003",
                           "span": span_of("D003", t, "Northstar Meridian Labs", 0)}]},
            {"id": "E009", "name": "Elena Vasquez", "type": "Person",
             "aliases": [],
             "mentions": [{"document_id": "D003",
                           "span": span_of("D003", t, "Elena Vasquez", 0)}]},
        ],
        "claims": [
            {"id": "C005", "subject_id": "E001", "predicate": "announced",
             "object": {"kind": "StringValue", "text": "joint metadata research partnership"},
             "polarity": "Positive", "confidence": 0.9,
             "document_id": "D003",
             "span": span_of("D003", t,
                             "Aster Vale Systems and Northstar Meridian Labs announced a joint\nmetadata research partnership"),
             "valid_from": None, "valid_to": None},
            {"id": "C006", "subject_id": "E005", "predicate": "present_at",
             "object": {"kind": "StringValue", "text": "Café Meridian"},
             "polarity": "Positive", "confidence": 0.85,
             "document_id": "D003",
             "span": span_of("D003", t,
                             "John Smith and Elena Vasquez attended the announcement"),
             "valid_from": None, "valid_to": None},
        ],
        "events": [],
        "citations": [
            {"citing": "D003", "cited": "D001", "span": None},
            {"citing": "D003", "cited": "D002", "span": None},
        ],
    }

    GOLD.mkdir(parents=True, exist_ok=True)
    for name, fragment in (("d001", d001), ("d002", d002), ("d003", d003)):
        out = GOLD / f"{name}.gold.json"
        out.write_text(json.dumps(fragment, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
        print(f"wrote {out.relative_to(ROOT)}")

    # self-check: every span in the fragments must slice exactly
    for fragment in (d001, d002, d003):
        doc_text = {d["id"]: d["normalized_text"] for d in fragment["documents"]}
        for entity in fragment["entities"]:
            for m in entity["mentions"]:
                text = doc_text[m["document_id"]]
                assert text[m["span"]["start"]:m["span"]["end"]] == m["span"]["quote"]
        for claim in fragment["claims"]:
            text = doc_text[claim["document_id"]]
            assert text[claim["span"]["start"]:claim["span"]["end"]] == claim["span"]["quote"]
    print("self-check ok: all spans slice exactly")
    return 0


if __name__ == "__main__":
    sys.exit(main())
