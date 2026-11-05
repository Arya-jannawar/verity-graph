#!/usr/bin/env python
"""Verity extraction runner.

Usage:
  python ai/run_extract.py --input data/sample --extractor mock \
      --out out/extraction.json [--gold-dir data/gold]

Exit codes: 0 success, 1 validation/semantic failure, 2 usage error.
The runner reads *.txt files from --input, normalizes them, runs the chosen
extractor, validates the result against ai/schema/extraction.schema.json
(jsonschema, draft 2020-12) plus semantic checks (code-point span exactness,
reference integrity, confidence bounds), and writes the JSON output.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

_REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(_REPO_ROOT / "ai"))

from extractors.base import BaseExtractor  # noqa: E402
from extractors.mock import MockExtractor  # noqa: E402
from readers.txt_reader import read_txt  # noqa: E402

SCHEMA_PATH = _REPO_ROOT / "ai" / "schema" / "extraction.schema.json"

EXIT_OK = 0
EXIT_VALIDATION = 1
EXIT_USAGE = 2


def build_extractor(name: str, documents: dict[str, str], args: argparse.Namespace) -> BaseExtractor:
    """Instantiates the named extractor (mock only in phase 1)."""
    if name == "mock":
        return MockExtractor(documents, args.gold_dir)
    available = ["mock"]
    raise SystemExit(
        f"unknown extractor '{name}'. Available extractors: {', '.join(available)}. "
        "Rule-based and LLM extractors arrive in later phases."
    )


def validate_semantics(record: dict) -> list[str]:
    """Code-point span exactness + reference integrity + confidence bounds."""
    errors: list[str] = []
    docs = {d["id"]: d["normalized_text"] for d in record["documents"]}
    entity_ids = {e["id"] for e in record["entities"]}

    def check_span(where: str, doc_id: str, span: dict) -> None:
        text = docs.get(doc_id)
        if text is None:
            errors.append(f"{where}: unknown document_id '{doc_id}'")
            return
        start, end, quote = span["start"], span["end"], span["quote"]
        if end < start:
            errors.append(f"{where}: span end < start")
            return
        actual = text[start:end]  # Python str indices are code points
        if actual != quote:
            errors.append(
                f"{where}: span quote mismatch — text slice is {actual!r}, quote is {quote!r}"
            )

    for entity in record["entities"]:
        for mention in entity["mentions"]:
            check_span(
                f"entity {entity['id']} mention",
                mention["document_id"],
                mention["span"],
            )

    for claim in record["claims"]:
        if claim["subject_id"] not in entity_ids:
            errors.append(f"claim {claim['id']}: unknown subject '{claim['subject_id']}'")
        obj = claim["object"]
        if obj["kind"] == "EntityRef" and obj["entity_id"] not in entity_ids:
            errors.append(f"claim {claim['id']}: unknown EntityRef '{obj['entity_id']}'")
        if claim["document_id"] not in docs:
            errors.append(f"claim {claim['id']}: unknown document '{claim['document_id']}'")
        check_span(f"claim {claim['id']}", claim["document_id"], claim["span"])
        if not 0.0 <= claim["confidence"] <= 1.0:
            errors.append(f"claim {claim['id']}: confidence outside [0,1]")

    for event in record["events"]:
        if event["entity_id"] not in entity_ids:
            errors.append(f"event {event['id']}: unknown entity '{event['entity_id']}'")
        if event["document_id"] not in docs:
            errors.append(f"event {event['id']}: unknown document '{event['document_id']}'")
        check_span(f"event {event['id']}", event["document_id"], event["span"])

    for citation in record["citations"]:
        if citation["citing"] not in docs:
            errors.append(f"citation: unknown citing document '{citation['citing']}'")
        if citation["cited"] not in docs:
            errors.append(f"citation: unknown cited document '{citation['cited']}'")

    return errors


def validate_schema(record: dict) -> list[str]:
    """Validates against the JSON Schema; returns all error messages."""
    try:
        import jsonschema
    except ImportError as exc:  # pragma: no cover - environment issue
        raise SystemExit(
            f"jsonschema is required for validation ({exc}). "
            "Create the venv and install ai/requirements.txt."
        ) from exc

    schema = json.loads(SCHEMA_PATH.read_text(encoding="utf-8"))
    validator = jsonschema.Draft202012Validator(schema)
    return [
        f"{list(error.absolute_path) or ['<root>']}: {error.message}"
        for error in sorted(validator.iter_errors(record), key=lambda e: str(e.absolute_path))
    ]


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Verity extraction runner")
    parser.add_argument("--input", required=True, help="directory with *.txt source documents")
    parser.add_argument("--extractor", required=True, choices=["mock"],
                        help="extraction strategy (mock replays gold by design)")
    parser.add_argument("--out", required=True, help="output extraction JSON path")
    parser.add_argument("--gold-dir", default=str(_REPO_ROOT / "data" / "gold"),
                        help="gold annotation directory (mock extractor)")
    args = parser.parse_args(argv)

    input_dir = Path(args.input)
    if not input_dir.is_dir():
        print(f"error: input directory not found: {input_dir}", file=sys.stderr)
        return EXIT_USAGE

    txt_files = sorted(input_dir.glob("*.txt"))
    if not txt_files:
        print(f"error: no .txt documents in {input_dir}", file=sys.stderr)
        return EXIT_USAGE

    documents = {path.name: read_txt(path) for path in txt_files}
    extractor = build_extractor(args.extractor, documents, args)
    record = extractor.extract()

    schema_errors = validate_schema(record)
    semantic_errors = validate_semantics(record)
    if schema_errors or semantic_errors:
        for error in schema_errors + semantic_errors:
            print(f"validation error: {error}", file=sys.stderr)
        return EXIT_VALIDATION

    out_path = Path(args.out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        json.dumps(record, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )

    print(
        f"extraction ok ({args.extractor}): {len(record['documents'])} documents, "
        f"{len(record['entities'])} entities, {len(record['claims'])} claims, "
        f"{len(record['events'])} events, {len(record['citations'])} citations"
    )
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
