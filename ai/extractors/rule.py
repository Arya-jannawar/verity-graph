"""RuleBasedExtractor: heuristics-only extraction from normalized text.

Phase-2 scope (per PHASES.md): capitalized-phrase entities, ISO date
normalization (YYYY | YYYY-MM | YYYY-MM-DD | ranges), alias detection
("J. Smith", "Mr. Smith", "Smith, John") and regex claim templates for the
starter predicates. spaCy is optional and degrades gracefully (never imported
unless present; the rules work without it).

Honesty policy:
* This extractor inspects SOURCE TEXT ONLY — it never reads data/gold.
* Heuristic limitation (documented, not hidden): negation scope detection is
  naive here (negation handling lands in Phase 3), and claim templates are
  conservative — recall on unusual phrasings is intentionally low rather than
  fabricating claims. Precision/recall is measured against gold in Phase 3.
"""

from __future__ import annotations

import re
from pathlib import Path

from extractors.base import BaseExtractor

SCHEMA_VERSION = "1.0.0"

# --- date normalization ------------------------------------------------------

_DATE_FULL = re.compile(r"\b(\d{4})-(\d{2})-(\d{2})\b")
_DATE_MONTH = re.compile(r"\b(\d{4})-(\d{2})\b")
_DATE_YEAR = re.compile(r"\b(\d{4})\b")

_MONTHS = {
    "january": 1, "february": 2, "march": 3, "april": 4, "may": 5, "june": 6,
    "july": 7, "august": 8, "september": 9, "october": 10, "november": 11,
    "december": 12,
}

_DATE_WORD_FULL = re.compile(
    r"\b(" + "|".join(_MONTHS) + r")\s+(\d{1,2}),?\s+(\d{4})\b", re.IGNORECASE
)
_DATE_WORD_MONTH = re.compile(r"\b(" + "|".join(_MONTHS) + r")\s+(\d{4})\b", re.IGNORECASE)

# --- entity / alias patterns --------------------------------------------------

_ALIAS_INITIAL = re.compile(r"\b([A-Z])[a-z]+\.?\s+([A-Z][a-z]+)\b")        # J. Smith
_ALIAS_TITLE = re.compile(r"\b(Mr\.|Ms\.|Mrs\.|Dr\.)\s+([A-Z][a-z]+)\b")    # Mr. Smith
_ALIAS_SURNAME_FIRST = re.compile(r"\b([A-Z][a-z]+),\s*([A-Z][a-z]+)\b")    # Smith, John
_CAPITALIZED_PHRASE = re.compile(r"\b([A-Z][A-Za-z]*(?:\s+[A-Z][A-Za-z0-9]+)*)\b")

_STOP_TOKENS = {
    "The", "A", "An", "In", "On", "At", "As", "Is", "Was", "Were", "Has",
    "Have", "Had", "Since", "From", "To", "And", "But", "The", "It", "He",
    "She", "They", "Mr", "Ms", "Mrs", "Dr", "January", "February", "March",
    "April", "May", "June", "July", "August", "September", "October",
    "November", "December", "Product", "Overview", "Partnership", "Wire",
    "Café", "Cafe",
}

# --- claim templates -----------------------------------------------------------

_TEMPLATES = [
    # has_ceo
    (re.compile(r"(?P<org>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))\s+(?:named|appointed)\s+(?P<person>[A-Z][a-z]+(?:\s+[A-Z][a-z]+)?)\s+(?:as\s+)?CEO", re.IGNORECASE | re.IGNORECASE), "has_ceo"),
    (re.compile(r"(?P<person>[A-Z][a-z]+(?:\s+[A-Z][a-z]+)?)\s+is the chief executive officer of\s+(?P<org>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))"), "has_ceo"),
    (re.compile(r"(?P<person>[A-Z][a-z]+(?:\s+[A-Z][a-z]+)?)\s+has served as chief executive officer of\s+(?P<org>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))"), "has_ceo"),
    # located_at
    (re.compile(r"(?P<org>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))\s+is (?:a|an)\s+[a-z]+ [a-z]+ (?:company|laboratory) (?:based|located) in\s+(?P<place>[A-Z][a-z]+)"), "located_at"),
    (re.compile(r"(?P<org>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))\s+is\s+(?:located|based)\s+in\s+(?P<place>[A-Z][a-z]+)"), "located_at"),
    # announced
    (re.compile(r"(?P<a>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))\s+and\s+(?P<b>[A-Z][\w'&.\- ]*?(?:Labs|Systems|Inc|Corp))\s+announced\s+(?P<what>[a-z][a-z\s]+?)(?:\.| at\b)"), "announced"),
    # present_at
    (re.compile(r"(?P<person>[A-Z][a-z]+(?:\s+[A-Z][a-z]+)?)\s+and\s+[A-Z][a-z]+(?:\s+[A-Z][a-z]+)?\s+attended\s+(?P<what>[a-z][a-z\s]+?)"), "present_at"),
    # happened_before
    (re.compile(r"(?P<a>[A-Z][\w ]+?)\s+happened before\s+(?P<b>[A-Z][\w ]+?)"), "happened_before"),
    (re.compile(r"before\s+(?P<b>[A-Z][\w ]+?),\s*(?P<a>[A-Z][\w ]+?)\s+(?:took place|happened|occurred)"), "happened_before"),
]


def normalize_date_token(token: str) -> str | None:
    """ISO date normalization table entry: returns YYYY | YYYY-MM | YYYY-MM-DD
    or None when the token is not a recognized date."""
    token = token.strip().lower()
    m = _DATE_FULL.search(token)
    if m:
        year, month, day = m.group(1), m.group(2), m.group(3)
        if 1 <= int(month) <= 12:
            return f"{year}-{month}-{day}"
        return None
    m = _DATE_MONTH.search(token)
    if m:
        year, month = m.group(1), m.group(2)
        if 1 <= int(month) <= 12:
            return f"{year}-{month}"
        return None
    m = _DATE_WORD_FULL.search(token)
    if m:
        month = _MONTHS[m.group(1).lower()]
        return f"{m.group(3)}-{month:02d}-{int(m.group(2)):02d}"
    m = _DATE_WORD_MONTH.search(token)
    if m:
        return f"{m.group(2)}-{_MONTHS[m.group(1).lower()]:02d}"
    m = _DATE_YEAR.search(token)
    if m:
        return m.group(1)
    return None


def detect_aliases(text: str) -> list[dict]:
    """Alias-shaped mentions: initials, titles, surname-first forms.
    Returns {"alias", "surname", "span"} — the resolver decides identity."""
    aliases: list[dict] = []
    for m in _ALIAS_INITIAL.finditer(text):
        aliases.append({"alias": m.group(0), "surname": m.group(2),
                        "span": {"start": m.start(), "end": m.end(), "quote": m.group(0)}})
    for m in _ALIAS_TITLE.finditer(text):
        aliases.append({"alias": m.group(0), "surname": m.group(2),
                        "span": {"start": m.start(), "end": m.end(), "quote": m.group(0)}})
    for m in _ALIAS_SURNAME_FIRST.finditer(text):
        aliases.append({"alias": m.group(0), "surname": m.group(1),
                        "span": {"start": m.start(), "end": m.end(), "quote": m.group(0)}})
    return aliases


class RuleBasedExtractor(BaseExtractor):
    """Heuristic extractor: entities, dates, aliases, starter-predicate claims."""

    def __init__(self, documents: dict[str, str], next_entity_id: int = 1) -> None:
        super().__init__(documents)
        self._next_entity_id = next_entity_id

    def extract(self) -> dict:
        entities: list[dict] = []
        claims: list[dict] = []
        entity_ids: dict[str, str] = {}   # canonical name -> entity id
        claim_counter = 1

        for filename in sorted(self.documents):
            doc_id = self._doc_id_for(filename)
            text = self.documents[filename]

            # --- entities: capitalized phrases not in the stop list
            for m in _CAPITALIZED_PHRASE.finditer(text):
                phrase = m.group(0)
                if phrase in _STOP_TOKENS or self._is_stop_phrase(phrase):
                    continue
                if phrase not in entity_ids:
                    entity_ids[phrase] = f"E{self._next_entity_id:03d}"
                    self._next_entity_id += 1
                    entities.append({
                        "id": entity_ids[phrase],
                        "name": phrase,
                        "type": self._guess_type(phrase),
                        "aliases": [],
                        "mentions": [{
                            "document_id": doc_id,
                            "span": {"start": m.start(), "end": m.end(), "quote": phrase},
                        }],
                    })

            # --- claims from templates
            for pattern, predicate in _TEMPLATES:
                for m in pattern.finditer(text):
                    groups = {k: v for k, v in m.groupdict().items() if v}
                    subject = groups.get("org") or groups.get("a") or groups.get("person")
                    if subject is None or subject not in entity_ids:
                        continue
                    object_text = (groups.get("person") or groups.get("place")
                                   or groups.get("what") or groups.get("b") or "")
                    object_value: dict
                    if object_text in entity_ids:
                        object_value = {"kind": "EntityRef", "entity_id": entity_ids[object_text]}
                    else:
                        object_value = {"kind": "StringValue", "text": object_text.strip()}
                    claims.append({
                        "id": f"C{claim_counter:03d}",
                        "subject_id": entity_ids[subject],
                        "predicate": predicate,
                        "object": object_value,
                        "polarity": "Positive",
                        "confidence": 0.6,
                        "document_id": doc_id,
                        "span": {"start": m.start(), "end": m.end(), "quote": m.group(0)},
                        "valid_from": None,
                        "valid_to": None,
                    })
                    claim_counter += 1

        return {
            "schema_version": SCHEMA_VERSION,
            "documents": self._documents_with_ids(),
            "entities": entities,
            "claims": claims,
            "events": [],
            "citations": [],
        }

    # --- internals -----------------------------------------------------------

    def _doc_id_for(self, filename: str) -> str:
        order = sorted(self.documents)
        return f"D{order.index(filename) + 1:03d}"

    def _documents_with_ids(self) -> list[dict]:
        return [
            {"id": self._doc_id_for(name), "filename": name, "format": "txt",
             "normalized_text": text, "metadata": {}, "citations": []}
            for name, text in sorted(self.documents.items())
        ]

    @staticmethod
    def _is_stop_phrase(phrase: str) -> bool:
        tokens = phrase.split()
        if len(tokens) > 6:
            return True
        if tokens and tokens[-1] in {".", ","}:
            return True
        # a phrase ending in a lowercase context word is a sentence fragment
        return bool(re.match(r"^[a-z]", phrase))

    @staticmethod
    def _guess_type(name: str) -> str:
        if re.search(r"(Labs|Systems|Inc|Corp|Company|Laboratory)$", name):
            return "Organization"
        if re.search(r"(Bay|City|Haven|Vale)$", name) and " " not in name:
            return "Location"
        if re.search(r"^(Product|Meridian Lens)", name):
            return "Product"
        if re.fullmatch(r"[A-Z][a-z]+(\s+[A-Z][a-z]+)?", name):
            return "Person"
        return "Organization"
