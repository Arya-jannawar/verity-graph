"""MockExtractor: replays hand-written gold annotations.

This extractor is BY DESIGN a gold replayer (the original team's phase-1
pipeline and its cross-process CLI demo). It never touches the source text
beyond bookkeeping: it loads ``*.gold.json`` fragments and merges them into
one extraction root. Every consumer treats its output as "mock extraction",
never as rule-based extraction quality evidence.
"""

from __future__ import annotations

import json
from pathlib import Path

from ai.extractors.base import BaseExtractor

SCHEMA_VERSION = "1.0.0"


class MockExtractor(BaseExtractor):
    """Merges gold fragments into a single extraction root."""

    def __init__(self, documents: dict[str, str], gold_dir: str | Path) -> None:
        super().__init__(documents)
        self.gold_dir = Path(gold_dir)

    def _gold_files(self) -> list[Path]:
        return sorted(self.gold_dir.glob("*.gold.json"))

    def extract(self) -> dict:
        result: dict = {
            "schema_version": SCHEMA_VERSION,
            "documents": [],
            "entities": [],
            "claims": [],
            "events": [],
            "citations": [],
        }
        seen_doc_ids: set[str] = set()
        entity_index: dict[str, dict] = {}
        seen_claim_ids: set[str] = set()
        seen_event_ids: set[str] = set()

        for gold_path in self._gold_files():
            fragment = json.loads(gold_path.read_text(encoding="utf-8"))

            for doc in fragment.get("documents", []):
                if doc["id"] in seen_doc_ids:
                    raise ValueError(f"duplicate document id across gold files: {doc['id']}")
                seen_doc_ids.add(doc["id"])
                result["documents"].append(doc)

            for entity in fragment.get("entities", []):
                eid = entity["id"]
                if eid not in entity_index:
                    entity_index[eid] = {
                        "id": eid,
                        "name": entity["name"],
                        "type": entity["type"],
                        "aliases": list(entity.get("aliases", [])),
                        "mentions": list(entity.get("mentions", [])),
                    }
                else:
                    existing = entity_index[eid]
                    if existing["type"] != entity["type"] or existing["name"] != entity["name"]:
                        raise ValueError(
                            f"entity {eid} has conflicting name/type across gold files"
                        )
                    existing["aliases"].extend(
                        a for a in entity.get("aliases", []) if a not in existing["aliases"]
                    )
                    existing["mentions"].extend(entity.get("mentions", []))

            for claim in fragment.get("claims", []):
                if claim["id"] in seen_claim_ids:
                    raise ValueError(f"duplicate claim id across gold files: {claim['id']}")
                seen_claim_ids.add(claim["id"])
                result["claims"].append(claim)

            for event in fragment.get("events", []):
                if event["id"] in seen_event_ids:
                    raise ValueError(f"duplicate event id across gold files: {event['id']}")
                seen_event_ids.add(event["id"])
                result["events"].append(event)

            result["citations"].extend(fragment.get("citations", []))

        result["entities"] = [entity_index[k] for k in sorted(entity_index)]
        return result
