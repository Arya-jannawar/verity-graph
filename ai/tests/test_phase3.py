"""Phase-3 AI tests: negation cue handling, LLM extractor with a FAKE client
(no network), strict span-discard behavior, and the honest precision/recall
report. Run from the repo root:
    .venv/Scripts/python.exe -I -m pytest ai/tests -v
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

_REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(_REPO_ROOT / "ai"))

from extractors.evaluation import format_report, score_extraction  # noqa: E402
from extractors.llm import (  # noqa: E402
    ExtractionError,
    LLMExtractor,
    build_llm_prompt,
    discard_invalid_claims,
    load_llm_config,
)

# ---------------------------------------------------------------- negation --

TEXT_POS = "Aster Vale Systems launched Product X on 2024-03-10."
TEXT_NEG = "Analysts reported Aster Vale Systems did not launch Product X on 2024-06-05."
TEXT_DENIAL = "The company denied that Product X shipped 8 modules."
TEXT_UNRELATED = ("The board did not meet in March, so Aster Vale Systems "
                  "launched Product X on 2024-03-10.")


def _run_rule(text: str) -> list[dict]:
    from extractors.rule import RuleBasedExtractor
    return RuleBasedExtractor({"d.txt": text}).extract()["claims"]


def test_negation_positive_claim_unaffected() -> None:
    claims = _run_rule(TEXT_POS)
    assert claims, "positive template should match"
    assert all(c["polarity"] == "Positive" for c in claims)


def test_negation_cue_flips_polarity() -> None:
    claims = _run_rule(TEXT_NEG)
    assert claims, "negated template should still match structurally"
    assert any(c["polarity"] == "Negative" for c in claims), claims


def test_negation_denial_phrase() -> None:
    claims = _run_rule(TEXT_DENIAL)
    # whatever matches, a nearby 'denied' cue must flip polarity
    for claim in claims:
        if claim["span"]["start"] > TEXT_DENIAL.index("denied"):
            assert claim["polarity"] == "Negative"


def test_negation_scope_limitation_is_published() -> None:
    """PUBLISHES the heuristic scope limitation (brief section 8): the naive
    60-character cue window leaks an UNRELATED earlier negation into a later
    claim's polarity. This test pins the honest behavior instead of hiding it;
    sentence-boundary scoping is future work and stated in LIMITATIONS.md."""
    claims = _run_rule(TEXT_UNRELATED)
    assert claims, "the launched template must match structurally"
    # the unrelated earlier 'did not meet' cue leaks into the later claim
    assert any(c["polarity"] == "Negative" for c in claims), (
        "expected the documented scope leak: the naive window cannot distinguish "
        "an unrelated negated clause from a negated claim"
    )


# -------------------------------------------------------------------- llm ---


def test_llm_config_missing_is_actionable() -> None:
    with pytest.raises(ExtractionError, match="VERITY_LLM_PROVIDER"):
        load_llm_config({})


def test_llm_config_reads_environment() -> None:
    config = load_llm_config({"VERITY_LLM_PROVIDER": "fake",
                              "VERITY_LLM_API_KEY": "test-key",
                              "VERITY_LLM_MODEL": "fake-model"})
    assert config == {"provider": "fake", "api_key": "test-key", "model": "fake-model"}


def test_llm_prompt_requests_strict_json() -> None:
    prompt = build_llm_prompt("hello world")
    assert "STRICT JSON" in prompt
    assert "hello world" in prompt


def _fake_llm_response_valid() -> str:
    text = "Aster Vale Systems launched Product X on 2024-03-10."
    start = text.index("Aster Vale Systems launched")
    end = start + len("Aster Vale Systems launched Product X")
    import json as _json
    return _json.dumps({
        "entities": [{"id": "E001", "name": "Aster Vale Systems",
                      "type": "Organization"}],
        "claims": [{"subject": "Aster Vale Systems", "predicate": "launched",
                    "object_text": "Product X", "polarity": "Positive",
                    "confidence": 0.9,
                    "span": {"start": start, "end": end,
                             "quote": text[start:end]},
                    "document_id": "D001"}],
    })


def test_llm_extractor_with_fake_client_no_network() -> None:
    text = "Aster Vale Systems launched Product X on 2024-03-10."
    calls: list[str] = []

    def fake_client(prompt: str) -> str:
        calls.append(prompt)
        return _fake_llm_response_valid()

    extractor = LLMExtractor({"d.txt": text},
                             {"provider": "fake", "api_key": "k", "model": "m"},
                             client=fake_client)
    record = extractor.extract()
    assert len(calls) == 1
    assert record["claims"], "valid spans must survive"
    assert record["claims"][0]["polarity"] == "Positive"
    assert record["entities"][0]["name"] == "Aster Vale Systems"


def test_llm_extractor_discards_span_mismatch() -> None:
    text = "Aster Vale Systems launched Product X on 2024-03-10."
    import json as _json
    # the claim quotes a DIFFERENT text than its span covers
    bad = _json.dumps({
        "entities": [{"id": "E001", "name": "Aster Vale Systems",
                      "type": "Organization"}],
        "claims": [{"subject": "Aster Vale Systems", "predicate": "launched",
                    "object_text": "Product X", "polarity": "Positive",
                    "confidence": 0.9,
                    "span": {"start": 0, "end": 10,
                             "quote": "WRONG QUOTE"},
                    "document_id": "D001"}],
    })

    def fake_client(prompt: str) -> str:
        return bad

    extractor = LLMExtractor({"d.txt": text},
                             {"provider": "fake", "api_key": "k", "model": "m"},
                             client=fake_client)
    record = extractor.extract()
    assert record["claims"] == [], "mismatched spans must be strictly discarded"


def test_discard_invalid_claims_rejects_ambiguous_without_reanchoring() -> None:
    text = "Acme said hello. Acme said hello again."
    claims = [{"document_id": "D001", "span": {"start": 0, "end": 15,
                                               "quote": "Acme said hello"}}]
    # the quote is exact at position 0 -> survives (first occurrence, explicit)
    assert discard_invalid_claims(claims, {"D001": text}) == claims
    # a quote that matches only a LATER position with wrong start -> discarded
    claims_bad = [{"document_id": "D001",
                   "span": {"start": 2, "end": 23, "quote": "Acme said hello again"}}]
    assert discard_invalid_claims(claims_bad, {"D001": text}) == []


def test_llm_extractor_non_json_output_is_actionable() -> None:
    def bad_client(prompt: str) -> str:
        return "I am not JSON"

    extractor = LLMExtractor({"d.txt": "text"},
                             {"provider": "fake", "api_key": "k", "model": "m"},
                             client=bad_client)
    with pytest.raises(ExtractionError, match="non-JSON"):
        extractor.extract()


def test_llm_extractor_retry_then_success() -> None:
    attempts: list[int] = []

    def flaky_client(prompt: str) -> str:
        attempts.append(1)
        if len(attempts) == 1:
            raise ConnectionError("transient")
        return _fake_llm_response_valid()

    extractor = LLMExtractor({"d.txt": "Aster Vale Systems launched Product X on 2024-03-10."},
                             {"provider": "fake", "api_key": "k", "model": "m"},
                             client=flaky_client)
    record = extractor.extract()
    assert len(attempts) == 2
    assert record["claims"]


# ---------------------------------------------------------------- P/R metrics


def test_precision_recall_exact_match() -> None:
    gold = {
        "entities": [{"id": "E001", "name": "Aster Vale Systems", "type": "Organization",
                      "aliases": [], "mentions": []},
                     {"id": "E002", "name": "Product X", "type": "Product",
                      "aliases": [], "mentions": []}],
        "claims": [
            {"id": "C001", "subject_id": "E001", "predicate": "launched",
             "object": {"kind": "EntityRef", "entity_id": "E002"},
             "polarity": "Positive", "confidence": 1.0},
        ],
    }
    predicted = {
        "entities": gold["entities"],
        "claims": [
            {"id": "C001", "subject_id": "E001", "predicate": "launched",
             "object": {"kind": "EntityRef", "entity_id": "E002"},
             "polarity": "Positive", "confidence": 0.9},
            {"id": "C002", "subject_id": "E001", "predicate": "has_ceo",
             "object": {"kind": "StringValue", "text": "nobody"},
             "polarity": "Positive", "confidence": 0.5},
        ],
    }
    scores = score_extraction(predicted, gold)
    assert scores["tp"] == 1
    assert scores["fp"] == 1
    assert scores["fn"] == 0
    assert abs(scores["precision"] - 0.5) < 1e-9
    assert abs(scores["recall"] - 1.0) < 1e-9
    assert "launched" in scores["per_predicate"]
    assert "has_ceo" in scores["per_predicate"]
    report = format_report(scores, "rule")
    assert "P=0.500" in report and "R=1.000" in report
    assert "must never be quoted" in report


def test_precision_recall_zero_division_guards() -> None:
    empty = {"entities": [], "claims": []}
    gold = {"entities": [], "claims": []}
    scores = score_extraction(empty, gold)
    assert scores["precision"] is None
    assert scores["recall"] is None
    assert scores["f1"] is None


def test_precision_recall_mock_is_labeled() -> None:
    """The mock extractor replays gold: its P/R is 1.0 BY CONSTRUCTION and the
    report must say so — never quote mock numbers as rule-based evidence."""
    from extractors.mock import MockExtractor
    from readers.txt_reader import read_txt
    documents = {p.name: read_txt(p)
                 for p in sorted((_REPO_ROOT / "data" / "sample").glob("*.txt"))}
    record = MockExtractor(documents, _REPO_ROOT / "data" / "gold").extract()
    # the mock replays ALL gold; scored against the SAME full gold it must
    # produce perfect numbers BY CONSTRUCTION (never real quality evidence)
    gold = record  # identity: gold == mock output by design
    scores = score_extraction(record, gold)
    assert scores["precision"] == 1.0 and scores["recall"] == 1.0
    assert "mock extractor replays gold" in format_report(scores, "mock")
