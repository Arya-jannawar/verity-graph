"""TXT reader: reads and normalizes raw document text.

Normalization contract (must match C++ ``verity::model::normalizeText`` in
``include/verity/model/Extraction.hpp``):

* CRLF and lone CR become LF.
* Text is composed to NFC.
* All trailing newline/space/tab characters are stripped and exactly one
  ``"\\n"`` is appended.

Spans produced downstream index CODE POINTS of this normalized text (Python
``str`` indices are code points, so plain slicing is correct; never encode to
bytes for span math).
"""

from __future__ import annotations

import unicodedata
from pathlib import Path


def normalize_text(raw: str) -> str:
    """Applies the frozen normalization contract to raw text."""
    text = raw.replace("\r\n", "\n").replace("\r", "\n")
    text = unicodedata.normalize("NFC", text)
    text = text.rstrip("\n \t") + "\n"
    return text


def read_txt(path: str | Path) -> str:
    """Reads a UTF-8 text file and returns its normalized text."""
    raw = Path(path).read_text(encoding="utf-8")
    return normalize_text(raw)
