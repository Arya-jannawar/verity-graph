"""Evaluation: precision/recall of an extraction against gold annotations.

Honesty rules (frozen):
* Rule-based extraction is compared against gold; the MOCK extractor is a
  gold replayer and is NEVER used as quality evidence.
* Matching is per (subject name, predicate, normalized object text); duplicates
  count once against the denominator they match first (greedy first-match on
  sorted keys, deterministic).
* TP/FP/FN with zero-division guards: empty predictions -> precision None
  (reported as "-"), empty gold -> recall None.
"""

from __future__ import annotations

import json
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent


def _claim_key(claim: dict, subjects_by_id: dict[str, str], entities_by_id: dict[str, str]) -> tuple | None:
    """Canonical match key: (canonical subject name, predicate, object text)."""
    subject = subjects_by_id.get(claim.get("subject_id", ""))
    if subject is None:
        return None
    obj = claim.get("object", {})
    kind = obj.get("kind")
    if kind == "EntityRef":
        obj_text = entities_by_id.get(obj.get("entity_id", ""), obj.get("entity_id", ""))
    elif kind == "DateValue":
        date = obj.get("date")
        obj_text = date if isinstance(date, str) else json.dumps(date, sort_keys=True)
    elif kind == "QuantityValue":
        obj_text = f"{obj.get('quantity')} {obj.get('unit')}"
    else:
        obj_text = obj.get("text", "")
    return (subject, claim.get("predicate", ""), str(obj_text))


def score_extraction(predicted: dict, gold: dict) -> dict:
    """Computes TP/FP/FN, precision, recall, F1 (aggregate and per-predicate)."""
    subjects_p = {e["id"]: e["name"] for e in predicted.get("entities", [])}
    subjects_g = {e["id"]: e["name"] for e in gold.get("entities", [])}
    entities_p = {e["id"]: e["name"] for e in predicted.get("entities", [])}
    entities_g = {e["id"]: e["name"] for e in gold.get("entities", [])}

    def keys_of(record: dict, subjects: dict, entities: dict) -> list[tuple]:
        keys = []
        for claim in record.get("claims", []):
            key = _claim_key(claim, subjects, entities)
            if key is not None:
                keys.append(key)
        return sorted(keys)

    predicted_keys = keys_of(predicted, subjects_p, entities_p)
    gold_keys = keys_of(gold, subjects_g, entities_g)

    # greedy deterministic matching over sorted keys
    gold_remaining = list(gold_keys)
    tp = 0
    matched_pred: list[tuple] = []
    for key in predicted_keys:
        if key in gold_remaining:
            gold_remaining.remove(key)
            tp += 1
            matched_pred.append(key)
    fp = len(predicted_keys) - tp
    fn = len(gold_keys) - tp

    precision = tp / (tp + fp) if (tp + fp) > 0 else None
    recall = tp / (tp + fn) if (tp + fn) > 0 else None
    f1 = (2 * precision * recall / (precision + recall)
          if precision is not None and recall is not None and (precision + recall) > 0
          else None)

    # per-predicate breakdown over gold predicate names
    per_predicate: dict[str, dict] = {}
    predicates = sorted({k[1] for k in gold_keys} | {k[1] for k in predicted_keys})
    for predicate in predicates:
        p_keys = [k for k in predicted_keys if k[1] == predicate]
        g_keys = [k for k in gold_keys if k[1] == predicate]
        tp_p = len(set(p_keys) & set(g_keys))
        fp_p = len(p_keys) - tp_p
        fn_p = len(g_keys) - tp_p
        per_predicate[predicate] = {
            "tp": tp_p, "fp": fp_p, "fn": fn_p,
            "precision": tp_p / (tp_p + fp_p) if (tp_p + fp_p) else None,
            "recall": tp_p / (tp_p + fn_p) if (tp_p + fn_p) else None,
        }

    return {
        "tp": tp, "fp": fp, "fn": fn,
        "precision": precision, "recall": recall, "f1": f1,
        "per_predicate": per_predicate,
    }


def score_extractor_files(extraction_path: str | Path, gold_dir: str | Path) -> dict:
    """Loads the predicted extraction JSON and all gold fragments, merges the
    gold the same way MockExtractor does, and scores."""
    predicted = json.loads(Path(extraction_path).read_text(encoding="utf-8"))

    gold: dict = {"entities": [], "claims": [], "events": [], "citations": []}
    for gold_file in sorted(Path(gold_dir).glob("*.gold.json")):
        fragment = json.loads(gold_file.read_text(encoding="utf-8"))
        gold["claims"].extend(fragment.get("claims", []))
        gold["events"].extend(fragment.get("events", []))
    # dedupe gold claims by id (fragments are disjoint by construction)
    seen: set[str] = set()
    unique_claims = []
    for claim in gold["claims"]:
        if claim["id"] not in seen:
            seen.add(claim["id"])
            unique_claims.append(claim)
    gold["claims"] = unique_claims
    # gold entity ids referenced by gold claims must resolve: collect names
    entity_names: dict[str, str] = {}
    for gold_file in sorted(Path(gold_dir).glob("*.gold.json")):
        fragment = json.loads(gold_file.read_text(encoding="utf-8"))
        for entity in fragment.get("entities", []):
            entity_names.setdefault(entity["id"], entity["name"])
    gold["entities"] = [{"id": k, "name": v, "type": "Person", "aliases": [],
                         "mentions": []} for k, v in sorted(entity_names.items())]

    return score_extraction(predicted, gold)


def format_report(scores: dict, extractor_name: str) -> str:
    """Human-readable honest report ('-' for undefined ratios)."""
    def pct(value: float | None) -> str:
        return "-" if value is None else f"{value:.3f}"

    lines = [f"precision/recall for '{extractor_name}' vs gold:"]
    lines.append(
        f"  aggregate: TP={scores['tp']} FP={scores['fp']} FN={scores['fn']} "
        f"P={pct(scores['precision'])} R={pct(scores['recall'])} F1={pct(scores['f1'])}"
    )
    for predicate, stats in sorted(scores["per_predicate"].items()):
        lines.append(
            f"  {predicate}: TP={stats['tp']} FP={stats['fp']} FN={stats['fn']} "
            f"P={pct(stats['precision'])} R={pct(stats['recall'])}"
        )
    lines.append(
        "  note: the mock extractor replays gold and must never be quoted as "
        "rule-based quality evidence"
    )
    return "\n".join(lines)
