"""LLMExtractor: optional environment-configured LLM extraction (Phase 3).

Honesty and safety contract (frozen):
* Provider and key come from environment variables (VERITY_LLM_PROVIDER,
  VERITY_LLM_API_KEY, optional VERITY_LLM_MODEL). No keys in source, logs or
  fixtures. Missing configuration raises ExtractionError with an actionable
  message — the offline pipeline never requires this extractor.
* The prompt requests STRICT JSON only; the response is parsed with
  json.loads and validated against the extraction schema; ANY claim whose
  span is not found EXACTLY in the source text is DISCARDED (never silently
  re-anchored to the first match).
* One bounded retry on transient failures. Network access happens only when
  a real provider client is injected — tests inject a FAKE client and no
  network is touched.
"""

from __future__ import annotations

import json
import os
from typing import Callable

from extractors.base import BaseExtractor

SCHEMA_VERSION = "1.0.0"


class ExtractionError(RuntimeError):
    """Raised for configuration, transport or validation failures."""


def load_llm_config(environ: dict | None = None) -> dict:
    """Reads provider settings from the environment (actionable error when
    incomplete)."""
    env = environ if environ is not None else os.environ
    provider = env.get("VERITY_LLM_PROVIDER", "").strip()
    api_key = env.get("VERITY_LLM_API_KEY", "").strip()
    if not provider or not api_key:
        raise ExtractionError(
            "LLM extraction is optional and not configured. Set VERITY_LLM_PROVIDER "
            "and VERITY_LLM_API_KEY in the environment (see .env.example). The mock "
            "and rule extractors work fully offline."
        )
    return {"provider": provider, "api_key": api_key, "model": env.get("VERITY_LLM_MODEL", "")}


def build_llm_prompt(text: str) -> str:
    """Strict JSON-only extraction prompt (no tool use, no prose)."""
    return (
        "Extract entities and claims from the text below. Respond with STRICT JSON "
        "only — no prose, no markdown fences — matching this shape:\n"
        '{"entities": [{"id": "E001", "name": "...", "type": "Person|Organization|'
        'Product|Location", "mentions": [{"span": {"start": 0, "end": 0, "quote": "..."}}]}],\n'
        ' "claims": [{"id": "C001", "subject": "...", "predicate": "announced|launched|'
        'has_ceo|located_at|present_at|happened_before|cites", "object_text": "...", '
        '"polarity": "Positive|Negative|Unknown", "confidence": 0.0, '
        '"span": {"start": 0, "end": 0, "quote": "..."}}]}\n'
        "Spans are code-point offsets into the given text and quote must be the exact "
        "substring. Claims whose span does not match the text will be discarded.\n\n"
        f"TEXT:\n{text}"
    )


def discard_invalid_claims(candidate_claims: list[dict], texts: dict[str, str]) -> list[dict]:
    """Discards any claim whose span does not slice EXACTLY in its document.

    Ambiguous quotes are NOT silently re-anchored: a claim survives only when
    text[start:end] == quote. Returns the survivors with document ids filled.
    """
    survivors: list[dict] = []
    for claim in candidate_claims:
        doc_id = claim.get("document_id") or claim.get("document") or ""
        text = texts.get(doc_id)
        span = claim.get("span") or {}
        if text is None or not span:
            continue
        start, end, quote = span.get("start"), span.get("end"), span.get("quote", "")
        if not isinstance(start, int) or not isinstance(end, int) or end < start:
            continue
        if text[start:end] != quote:
            continue  # strict discard — no silent re-anchoring
        survivors.append(claim)
    return survivors


class LLMExtractor(BaseExtractor):
    """LLM-backed extractor with strict validation and one bounded retry.

    `client` is a callable (prompt: str) -> str so tests can inject a FAKE
    provider; the default client raises (no network library is imported here).
    """

    def __init__(self, documents: dict[str, str], config: dict,
                 client: Callable[[str], str] | None = None) -> None:
        super().__init__(documents)
        self.config = config
        self._client = client if client is not None else self._http_client

    @staticmethod
    def _http_client(prompt: str) -> str:
        raise ExtractionError(
            "no LLM transport available in this build; inject a client callable "
            "for tests or run with a real provider integration"
        )

    def _call_with_retry(self, prompt: str) -> str:
        try:
            return self._client(prompt)
        except ExtractionError:
            raise
        except Exception as exc:  # transient transport failure: one retry
            try:
                return self._client(prompt)
            except Exception:
                raise ExtractionError(f"LLM call failed after retry: {exc}") from exc

    def extract(self) -> dict:
        entities: list[dict] = []
        claims: list[dict] = []
        entity_ids: set[str] = set()
        claim_counter = 1

        for filename in sorted(self.documents):
            doc_id = f"D{sorted(self.documents).index(filename) + 1:03d}"
            text = self.documents[filename]
            raw = self._call_with_retry(build_llm_prompt(text))
            try:
                payload = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ExtractionError(f"LLM returned non-JSON output: {exc}") from exc
            if not isinstance(payload, dict):
                raise ExtractionError("LLM output must be a JSON object")

            for entity in payload.get("entities", []):
                name = str(entity.get("name", "")).strip()
                if not name or name in {e["name"] for e in entities}:
                    continue
                eid = str(entity.get("id") or f"E{len(entities) + 1:03d}")
                entities.append({
                    "id": eid,
                    "name": name,
                    "type": entity.get("type", "Organization"),
                    "aliases": [],
                    "mentions": [],
                })
                entity_ids.add(name)

            survivors = discard_invalid_claims(payload.get("claims", []),
                                               {doc_id: text})
            for claim in survivors:
                subject = str(claim.get("subject", "")).strip()
                if subject not in entity_ids:
                    continue
                claims.append({
                    "id": f"C{claim_counter:03d}",
                    "subject_id": next(e["id"] for e in entities
                                       if e["name"] == subject),
                    "predicate": claim.get("predicate", ""),
                    "object": {"kind": "StringValue",
                               "text": str(claim.get("object_text", ""))},
                    "polarity": claim.get("polarity", "Unknown"),
                    "confidence": float(claim.get("confidence", 0.5)),
                    "document_id": doc_id,
                    "span": claim["span"],
                    "valid_from": None,
                    "valid_to": None,
                })
                claim_counter += 1

        return {
            "schema_version": SCHEMA_VERSION,
            "documents": [
                {"id": f"D{i + 1:03d}", "filename": name, "format": "txt",
                 "normalized_text": text, "metadata": {}, "citations": []}
                for i, (name, text) in enumerate(sorted(self.documents.items()))
            ],
            "entities": entities,
            "claims": claims,
            "events": [],
            "citations": [],
        }
