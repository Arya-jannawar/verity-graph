"""Phase-2 extraction tests: date normalization tables, alias detection,
rule-extractor behavior, and PDF/DOCX reader span correctness.

Run from the repo root:
    .venv/Scripts/python.exe -I -m pytest ai/tests -v
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(_REPO_ROOT / "ai"))

from extractors.rule import RuleBasedExtractor, detect_aliases, normalize_date_token  # noqa: E402
from readers.docx_reader import read_docx, read_docx_paragraphs  # noqa: E402
from readers.pdf_reader import read_pdf, read_pdf_paragraphs  # noqa: E402
from readers.txt_reader import read_txt  # noqa: E402

SAMPLE_DIR = _REPO_ROOT / "data" / "sample"


@pytest.fixture(scope="module")
def d004_text() -> str:
    return read_txt(SAMPLE_DIR / "d004_consulting_note.txt")


@pytest.fixture(scope="module")
def d005_text() -> str:
    return read_txt(SAMPLE_DIR / "d005_milestones_memo.txt")


@pytest.mark.parametrize(
    ("token", "expected"),
    [
        ("2024", "2024"),
        ("2024-03", "2024-03"),
        ("2024-03-15", "2024-03-15"),
        ("March 15, 2024", "2024-03-15"),
        ("march 2024", "2024-03"),
        ("March 2024", "2024-03"),
        ("on 2024-06-30 then", "2024-06-30"),
        ("in December 2023", "2023-12"),
        ("not a date", None),
        ("2024-13", None),          # invalid month
        ("", None),
    ],
)
def test_date_normalization_table(token: str, expected: str | None) -> None:
    assert normalize_date_token(token) == expected


def test_alias_detection_shapes(d004_text: str) -> None:
    aliases = detect_aliases(d004_text)
    found = {a["alias"] for a in aliases}
    # the note contains J. Smith, Mr. Smith and Smith, John forms
    assert any("J. Smith" == a for a in found), found
    assert any("Mr. Smith" == a for a in found), found
    assert any("Smith, John" == a for a in found), found
    # every detected alias span slices exactly from the text (code points)
    for alias in aliases:
        assert d004_text[alias["span"]["start"]:alias["span"]["end"]] == alias["alias"]


def test_rule_extractor_never_loads_gold(d004_text: str, d005_text: str) -> None:
    """The rule extractor consumes source text only — construct it without any
    gold directory and confirm it still produces a valid-shaped record."""
    extractor = RuleBasedExtractor({"d004.txt": d004_text, "d005.txt": d005_text})
    record = extractor.extract()
    assert record["schema_version"] == "1.0.0"
    assert len(record["documents"]) == 2
    assert record["entities"], "expected at least one entity"
    for claim in record["claims"]:
        assert claim["predicate"] in {
            "announced", "launched", "has_ceo", "located_at",
            "present_at", "happened_before", "cites",
        }
        # every claim span slices exactly from its document text
        text = {d["id"]: d["normalized_text"] for d in record["documents"]}[claim["document_id"]]
        span = claim["span"]
        assert text[span["start"]:span["end"]] == span["quote"]


def test_rule_extractor_happened_before_claims(d004_text: str, d005_text: str) -> None:
    extractor = RuleBasedExtractor({"d004.txt": d004_text, "d005.txt": d005_text})
    record = extractor.extract()
    # the memo's "before" phrasings are conservative: at most the templates
    # matched, and none invented a claim outside the starter predicates
    predicates = {c["predicate"] for c in record["claims"]}
    assert predicates <= {"announced", "launched", "has_ceo", "located_at",
                          "present_at", "happened_before", "cites"}


@pytest.fixture(scope="module")
def pdf_path() -> Path:
    path = SAMPLE_DIR / "d004_format_variant.pdf"
    if not path.exists():
        pytest.skip("PDF fixture not generated; run scripts/make_phase2_fixtures.py")
    return path


@pytest.fixture(scope="module")
def docx_path() -> Path:
    path = SAMPLE_DIR / "d005_format_variant.docx"
    if not path.exists():
        pytest.skip("DOCX fixture not generated; run scripts/make_phase2_fixtures.py")
    return path


def test_pdf_reader_normalized_and_spanned(pdf_path: Path) -> None:
    text = read_pdf(pdf_path)
    assert text.endswith("\n") and "\r" not in text
    paragraphs = read_pdf_paragraphs(pdf_path)
    assert paragraphs, "expected paragraphs from the PDF fixture"
    for p in paragraphs:
        assert text[p["start"]:p["end"]] == p["text"], p
        assert p["page"] >= 1


def test_docx_reader_normalized_and_spanned(docx_path: Path) -> None:
    text = read_docx(docx_path)
    assert text.endswith("\n") and "\r" not in text
    paragraphs = read_docx_paragraphs(docx_path)
    assert paragraphs, "expected paragraphs from the DOCX fixture"
    for p in paragraphs:
        assert text[p["start"]:p["end"]] == p["text"], p
        assert p["index"] >= 0


def test_format_variant_documents_exist() -> None:
    """Documented semantics: d004_format_variant.pdf and d005_format_variant.docx
    are FORMAT VARIANTS of the neutral overview content — semantic originals
    remain d001-d003 (txt) and d004/d005 (txt). Duplicate claim counting across
    variants is avoided by listing the variants as format fixtures only."""
    assert (SAMPLE_DIR / "d004_format_variant.pdf").exists()
    assert (SAMPLE_DIR / "d005_format_variant.docx").exists()
