"""Phase-1 extraction tests: schema validity, exact code-point spans, mock
merging, semantic reference integrity, and normalization parity with the C++
reader contract.

Run from the repo root:
    .venv/Scripts/python.exe -I -m pytest ai/tests -v
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(_REPO_ROOT / "ai"))

from extractors.mock import MockExtractor  # noqa: E402
from readers.txt_reader import normalize_text, read_txt  # noqa: E402
from run_extract import validate_schema, validate_semantics  # noqa: E402

SAMPLE_DIR = _REPO_ROOT / "data" / "sample"
GOLD_DIR = _REPO_ROOT / "data" / "gold"


@pytest.fixture(scope="module")
def documents() -> dict[str, str]:
    txts = sorted(SAMPLE_DIR.glob("*.txt"))
    assert len(txts) == 5, f"expected 5 sample documents, found {len(txts)}"
    return {p.name: read_txt(p) for p in txts}


@pytest.fixture(scope="module")
def record(documents: dict[str, str]) -> dict:
    return MockExtractor(documents, GOLD_DIR).extract()


def test_schema_validates_mock_output(record: dict) -> None:
    errors = validate_schema(record)
    assert not errors, "\n".join(errors)
    semantic = validate_semantics(record)
    assert not semantic, "\n".join(semantic)


def test_all_gold_spans_exact(documents: dict[str, str]) -> None:
    """Every shipped gold span must slice exactly (code points, not bytes)."""
    gold_files = sorted(GOLD_DIR.glob("*.gold.json"))
    assert len(gold_files) == 3
    checked = 0
    for gold in gold_files:
        fragment = json.loads(gold.read_text(encoding="utf-8"))
        texts = {d["id"]: d["normalized_text"] for d in fragment["documents"]}
        for entity in fragment["entities"]:
            for mention in entity["mentions"]:
                text = texts[mention["document_id"]]
                span = mention["span"]
                assert text[span["start"]:span["end"]] == span["quote"], (
                    f"{gold.name} entity {entity['id']} span mismatch"
                )
                checked += 1
        for claim in fragment["claims"]:
            text = texts[claim["document_id"]]
            span = claim["span"]
            assert text[span["start"]:span["end"]] == span["quote"], (
                f"{gold.name} claim {claim['id']} span mismatch"
            )
            checked += 1
    assert checked >= 15, f"expected >= 15 spans, checked {checked}"


def test_utf8_code_points_not_bytes(documents: dict[str, str]) -> None:
    """D003 contains 'Café' — code-point span math must differ from bytes."""
    text = documents["d003_partnership_wire.txt"]
    assert "Café Meridian" in text
    code_points = len(text)
    byte_len = len(text.encode("utf-8"))
    assert code_points != byte_len, (
        "fixture lost its non-ASCII character; the code-point/byte distinction is no longer exercised"
    )
    # the é is one code point but two UTF-8 bytes
    assert len("é") == 1 and len("é".encode("utf-8")) == 2
    idx = text.index("Café Meridian")
    assert text[idx:idx + len("Café Meridian")] == "Café Meridian"


def test_mock_merges_documents(record: dict) -> None:
    assert record["schema_version"] == "1.0.0"
    assert len(record["documents"]) == 5
    assert len(record["entities"]) == 12
    assert len(record["claims"]) == 9
    assert len(record["events"]) == 3
    assert len(record["citations"]) == 2
    # entity ids merged across fragments stay unique and sorted
    ids = [e["id"] for e in record["entities"]]
    assert ids == sorted(set(ids))


def test_semantic_reference_integrity(record: dict) -> None:
    entity_ids = {e["id"] for e in record["entities"]}
    doc_ids = {d["id"] for d in record["documents"]}
    for claim in record["claims"]:
        assert claim["subject_id"] in entity_ids
        assert claim["document_id"] in doc_ids
        obj = claim["object"]
        if obj["kind"] == "EntityRef":
            assert obj["entity_id"] in entity_ids
    for citation in record["citations"]:
        assert citation["citing"] in doc_ids
        assert citation["cited"] in doc_ids


def test_normalization_matches_cpp_contract() -> None:
    """C++ normalizeText parity table (include/verity/model/Extraction.hpp)."""
    assert normalize_text("a\r\nb\r c\n") == "a\nb\n c\n"
    assert normalize_text("text\n\n\n") == "text\n"
    assert normalize_text("text  \n") == "text\n"
    assert normalize_text("no trailing newline") == "no trailing newline\n"
    assert normalize_text("") == "\n"
    assert normalize_text("keeps interior   spaces\nsecond\n") == (
        "keeps interior   spaces\nsecond\n"
    )
    # NFC composition: e + combining acute becomes é (one code point)
    decomposed = "Cafe\u0301"
    assert len(decomposed) == 5
    assert len(normalize_text(decomposed)) == 5  # "Café\n" = 5 code points
    assert normalize_text(decomposed).endswith("é\n")


def test_mock_extractor_rejects_duplicate_claim_ids(tmp_path: Path) -> None:
    """Duplicate claim ids across gold fragments must be a loud error."""
    claim = {"id": "C090", "subject_id": "E001", "predicate": "has_ceo",
             "object": {"kind": "StringValue", "text": "x"},
             "polarity": "Positive", "confidence": 0.5,
             "document_id": "D090",
             "span": {"start": 0, "end": 0, "quote": ""},
             "valid_from": None, "valid_to": None}

    def make_fragment(doc_id: str) -> dict:
        return {
            "schema_version": "1.0.0",
            "documents": [{"id": doc_id, "filename": f"{doc_id.lower()}.txt", "format": "txt",
                           "normalized_text": "x\n", "metadata": {}, "citations": []}],
            "entities": [], "events": [], "citations": [],
        }

    (tmp_path / "a.gold.json").write_text(
        json.dumps({**make_fragment("D090"), "claims": [claim]}), encoding="utf-8")
    (tmp_path / "b.gold.json").write_text(
        json.dumps({**make_fragment("D091"), "claims": [dict(claim)]}), encoding="utf-8")
    with pytest.raises(ValueError, match="duplicate claim id"):
        MockExtractor({"x.txt": "x\n"}, tmp_path).extract()
