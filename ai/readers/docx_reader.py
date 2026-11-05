"""DOCX reader: extracts paragraph text with python-docx and normalizes it.

Same normalization/offset contract as pdf_reader (docs/CONTRACT.md section 6).
"""

from __future__ import annotations

from pathlib import Path

from readers.txt_reader import normalize_text


def read_docx(path: str | Path) -> str:
    """Extracts all paragraphs and returns the normalized document text."""
    try:
        import docx
    except ImportError as exc:  # pragma: no cover - environment issue
        raise SystemExit(
            "python-docx is required for DOCX reading; install ai/requirements.txt"
        ) from exc
    document = docx.Document(str(path))
    texts = [p.text for p in document.paragraphs]
    return normalize_text("\n".join(texts))


def read_docx_paragraphs(path: str | Path) -> list[dict]:
    """Returns normalized paragraphs with code-point offsets into the
    normalized document text; `index` is the python-docx paragraph index."""
    try:
        import docx
    except ImportError as exc:  # pragma: no cover - environment issue
        raise SystemExit("python-docx is required for DOCX reading") from exc
    document = docx.Document(str(path))
    raw = [p.text for p in document.paragraphs]
    normalized_document = normalize_text("\n".join(raw))

    paragraphs: list[dict] = []
    cursor = 0
    for index, paragraph_text in enumerate(raw):
        chunk = paragraph_text.strip()
        if not chunk:
            continue
        start = normalized_document.find(chunk, cursor)
        if start < 0:
            continue
        paragraphs.append(
            {"text": chunk, "start": start, "end": start + len(chunk), "index": index}
        )
        cursor = start + len(chunk)
    return paragraphs
