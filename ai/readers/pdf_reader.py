"""PDF reader: extracts text per page with pypdf and normalizes it.

The normalized text follows the frozen contract (CRLF->LF, NFC, one trailing
newline). Paragraph metadata records page numbers; offsets are code points in
the NORMALIZED text (PDF page/paragraph refs point into normalized text, not
binary bytes — see docs/CONTRACT.md section 6).
"""

from __future__ import annotations

from pathlib import Path

from readers.txt_reader import normalize_text


def read_pdf(path: str | Path) -> str:
    """Extracts all pages' text and returns the normalized document text."""
    try:
        from pypdf import PdfReader
    except ImportError as exc:  # pragma: no cover - environment issue
        raise SystemExit("pypdf is required for PDF reading; install ai/requirements.txt") from exc
    reader = PdfReader(str(path))
    pages = [page.extract_text() or "" for page in reader.pages]
    return normalize_text("\n".join(pages))


def read_pdf_paragraphs(path: str | Path) -> list[dict]:
    """Returns normalized paragraphs with their code-point offsets.

    Each item: {"text", "start", "end", "page"} where start/end index the
    document's normalized text (code points) and page is the 1-based PDF page.
    """
    try:
        from pypdf import PdfReader
    except ImportError as exc:  # pragma: no cover - environment issue
        raise SystemExit("pypdf is required for PDF reading") from exc
    reader = PdfReader(str(path))
    # First pass: normalized page texts joined with newlines = document text.
    raw_pages = [(page.extract_text() or "").strip() for page in reader.pages]
    normalized_pages = [normalize_text(t) for t in raw_pages]
    document = normalize_text("\n".join(raw_pages))

    paragraphs: list[dict] = []
    cursor = 0
    for page_number, page_text in enumerate(normalized_pages, start=1):
        # The page's normalized text must appear verbatim inside the document
        # text (the join preserves order and separators).
        idx = document.find(page_text, cursor)
        if idx < 0:
            continue  # defensive: malformed PDF layout
        cursor = idx
        for chunk in page_text.split("\n"):
            chunk = chunk.strip()
            if not chunk:
                continue
            start = document.find(chunk, cursor)
            if start < 0:
                continue
            paragraphs.append(
                {"text": chunk, "start": start, "end": start + len(chunk), "page": page_number}
            )
            cursor = start + len(chunk)
    return paragraphs
