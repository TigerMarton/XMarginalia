"""EPUB parser, codepoint-aligned HTML chunker, and KOReader partial-MD5 hash.

Extracts book identity keys (UUID, ISBN, ASIN, ko_md5, title, author, filename)
and chunks each spine item while tracking both:
  * chapter-local Unicode codepoint offsets (`char_start`, `char_end`) matching
    `Section::getVisibleTextOffsetForPage()` on the ESP32;
  * book-wide cumulative character offsets (`global_char_start`, `global_char_end`)
    so whole-book progress percentage fallbacks remain spoiler-safe even when
    chapters were split or merged by Calibre.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import hashlib
from html.parser import HTMLParser
import os
import pathlib
import posixpath
import re
import xml.etree.ElementTree as ET
import zipfile

KOREADER_CHUNK_SIZE = 1024
KOREADER_OFFSET_COUNT = 11

NON_VISIBLE_TAGS = frozenset({"head", "style", "script", "title", "rp"})
BLOCK_TAGS = frozenset(
    {
        "h1",
        "h2",
        "h3",
        "h4",
        "h5",
        "h6",
        "p",
        "li",
        "div",
        "br",
        "blockquote",
        "ul",
        "ol",
        "pre",
        "figure",
        "tr",
        "section",
        "article",
    }
)


@dataclass
class TextChunk:
    canonical_book_id: str
    spine_index: int
    spine_href: str
    chapter_title: str
    chunk_index: int
    char_start: int
    char_end: int
    chapter_total_chars: int
    global_char_start: int
    global_char_end: int
    book_total_chars: int
    text: str


@dataclass
class SpineDocument:
    spine_index: int
    spine_href: str
    chapter_title: str
    visible_text: str
    paragraphs: list[tuple[int, int, str]] = field(default_factory=list)


@dataclass
class ParsedBook:
    canonical_book_id: str
    title: str
    author: str
    language: str
    package_id: str
    identifiers: list[str]
    isbn: str
    asin: str
    ko_md5: str
    filename: str
    total_chars: int
    spine_docs: list[SpineDocument]
    chunks: list[TextChunk]


def calculate_koreader_md5(file_path: str | pathlib.Path) -> str:
    """Compute the exact partial-MD5 hash used by KOReaderDocumentId::calculate()."""
    p = pathlib.Path(file_path)
    if not p.is_file():
        return ""
    file_size = p.stat().st_size
    md5 = hashlib.md5()
    with p.open("rb") as f:
        for i in range(-1, KOREADER_OFFSET_COUNT - 1):
            offset = 0 if i < 0 else (KOREADER_CHUNK_SIZE << (2 * i))
            if offset >= file_size:
                continue
            f.seek(offset, os.SEEK_SET)
            data = f.read(min(KOREADER_CHUNK_SIZE, file_size - offset))
            if data:
                md5.update(data)
    return md5.hexdigest()


def normalize_whitespace(text: str) -> str:
    """Match ChapterHtmlSlimParser::trimAndNormalize()."""
    return re.sub(r"[ \r\n\t]+", " ", text).strip()


class _VisibleTextExtractor(HTMLParser):
    """Extracts visible paragraphs and their Unicode codepoint offsets."""

    def __init__(self) -> None:
        super().__init__(convert_charrefs=True)
        self._skip_depth = 0
        self._current_parts: list[str] = []
        self.paragraphs: list[tuple[int, int, str]] = []
        self._cursor = 0

    def _flush_block(self) -> None:
        if not self._current_parts:
            return
        raw = "".join(self._current_parts)
        self._current_parts.clear()
        norm = normalize_whitespace(raw)
        if not norm:
            return
        start = self._cursor
        end = start + len(norm)
        self.paragraphs.append((start, end, norm))
        # Account for the inter-paragraph space separator in cumulative visible text
        self._cursor = end + 1

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        t = tag.lower()
        if t in NON_VISIBLE_TAGS:
            self._skip_depth += 1
            return
        if self._skip_depth == 0 and t in BLOCK_TAGS:
            self._flush_block()

    def handle_endtag(self, tag: str) -> None:
        t = tag.lower()
        if t in NON_VISIBLE_TAGS:
            if self._skip_depth > 0:
                self._skip_depth -= 1
            return
        if self._skip_depth == 0 and t in BLOCK_TAGS:
            self._flush_block()

    def handle_data(self, data: str) -> None:
        if self._skip_depth == 0 and data:
            self._current_parts.append(data)

    def close(self) -> None:
        super().close()
        self._flush_block()


def extract_visible_paragraphs(html_text: str) -> tuple[str, list[tuple[int, int, str]]]:
    """Return `(full_visible_text, [(char_start, char_end, paragraph_text), ...])`."""
    parser = _VisibleTextExtractor()
    parser.feed(html_text)
    parser.close()
    full_text = " ".join(p[2] for p in parser.paragraphs)
    return full_text, parser.paragraphs


def _local_name(tag: str) -> str:
    return tag.split("}", 1)[-1] if "}" in tag else tag


def _extract_isbn_or_asin(raw_id: str, scheme: str = "") -> tuple[str, str]:
    clean = raw_id.strip()
    low = clean.lower()
    scheme_low = scheme.lower()
    if low.startswith("urn:isbn:"):
        return clean[9:].strip(), ""
    if scheme_low == "isbn":
        return clean, ""
    if low.startswith("urn:asin:") or low.startswith("asin:"):
        return "", clean.split(":", 1)[-1].strip()
    if scheme_low in ("asin", "mobi-asin", "amazon"):
        return "", clean
    digits = re.sub(r"[^0-9Xx]", "", clean)
    if len(digits) in (10, 13) and digits[:-1].isdigit():
        return digits.upper(), ""
    return "", ""


def _parse_toc_titles(zf: zipfile.ZipFile, opf_dir: str, manifest: dict[str, tuple[str, str]]) -> dict[str, str]:
    """Extract href -> chapter title from EPUB2 NCX or EPUB3 nav document."""
    titles: dict[str, str] = {}
    for _item_id, (href, media_type) in manifest.items():
        if media_type == "application/x-dtbncx+xml" or href.lower().endswith(".ncx"):
            try:
                ncx_xml = zf.read(href)
                root = ET.fromstring(ncx_xml)
                ncx_dir = posixpath.dirname(href)
                for nav_point in root.iter():
                    if _local_name(nav_point.tag) != "navPoint":
                        continue
                    label_text = ""
                    content_src = ""
                    for child in nav_point:
                        cname = _local_name(child.tag)
                        if cname == "navLabel":
                            for sub in child.iter():
                                if _local_name(sub.tag) == "text" and sub.text:
                                    label_text = normalize_whitespace(sub.text)
                                    break
                        elif cname == "content":
                            content_src = child.attrib.get("src", "")
                    if label_text and content_src:
                        base_src = content_src.split("#", 1)[0]
                        norm_href = posixpath.normpath(posixpath.join(ncx_dir, base_src)) if ncx_dir else posixpath.normpath(base_src)
                        titles.setdefault(norm_href, label_text)
            except Exception:
                continue
        elif "html" in media_type and ("nav" in href.lower() or "toc" in href.lower()):
            try:
                nav_html = zf.read(href).decode("utf-8", errors="replace")
                nav_dir = posixpath.dirname(href)
                for m in re.finditer(r'<a[^>]+href=["\']([^"\']+)["\'][^>]*>(.*?)</a>', nav_html, re.I | re.S):
                    raw_href, raw_label = m.group(1), m.group(2)
                    clean_label = normalize_whitespace(re.sub(r"<[^>]+>", "", raw_label))
                    base_src = raw_href.split("#", 1)[0]
                    if clean_label and base_src:
                        norm_href = posixpath.normpath(posixpath.join(nav_dir, base_src)) if nav_dir else posixpath.normpath(base_src)
                        titles.setdefault(norm_href, clean_label)
            except Exception:
                continue
    return titles


def chunk_spine_paragraphs(
    canonical_book_id: str,
    spine_doc: SpineDocument,
    global_offset_base: int,
    book_total_chars: int,
    target_words: int = 240,
    overlap_words: int = 40,
) -> list[TextChunk]:
    """Group paragraphs into codepoint-aligned chunks without crossing spine boundaries."""
    paragraphs = spine_doc.paragraphs
    chapter_total_chars = len(spine_doc.visible_text)
    if not paragraphs:
        return []

    chunks: list[TextChunk] = []
    idx = 0
    chunk_idx = 0
    n = len(paragraphs)

    while idx < n:
        start_idx = idx
        char_start = paragraphs[start_idx][0]
        char_end = paragraphs[start_idx][1]
        texts: list[str] = []
        word_count = 0

        while idx < n:
            p_start, p_end, p_text = paragraphs[idx]
            p_words = len(p_text.split())
            if texts and word_count + p_words > target_words:
                break
            texts.append(p_text)
            char_end = p_end
            word_count += p_words
            idx += 1

            # If a single paragraph is huge (> target_words), split it by sentences/words
            # while maintaining exact codepoint offsets!
            if len(texts) == 1 and word_count > target_words * 1.4:
                chunks.extend(
                    _split_large_paragraph(
                        canonical_book_id=canonical_book_id,
                        spine_doc=spine_doc,
                        p_start=p_start,
                        p_text=p_text,
                        start_chunk_idx=chunk_idx,
                        chapter_total_chars=chapter_total_chars,
                        global_offset_base=global_offset_base,
                        book_total_chars=book_total_chars,
                        target_words=target_words,
                        overlap_words=overlap_words,
                    )
                )
                chunk_idx = len(chunks)
                texts.clear()
                break

        if texts:
            joined = " ".join(texts)
            chunks.append(
                TextChunk(
                    canonical_book_id=canonical_book_id,
                    spine_index=spine_doc.spine_index,
                    spine_href=spine_doc.spine_href,
                    chapter_title=spine_doc.chapter_title,
                    chunk_index=chunk_idx,
                    char_start=char_start,
                    char_end=char_end,
                    chapter_total_chars=chapter_total_chars,
                    global_char_start=global_offset_base + char_start,
                    global_char_end=global_offset_base + char_end,
                    book_total_chars=book_total_chars,
                    text=joined,
                )
            )
            chunk_idx += 1

        # Step back by overlap_words if we haven't reached the end of the chapter
        if idx < n and idx - start_idx > 1 and overlap_words > 0:
            back_words = 0
            back_idx = idx
            while back_idx - 1 > start_idx:
                w = len(paragraphs[back_idx - 1][2].split())
                if back_words + w > overlap_words:
                    break
                back_words += w
                back_idx -= 1
            idx = back_idx

    return chunks


def _split_large_paragraph(
    canonical_book_id: str,
    spine_doc: SpineDocument,
    p_start: int,
    p_text: str,
    start_chunk_idx: int,
    chapter_total_chars: int,
    global_offset_base: int,
    book_total_chars: int,
    target_words: int,
    overlap_words: int,
) -> list[TextChunk]:
    """Split an oversized paragraph on word boundaries while preserving exact offsets."""
    word_matches = list(re.finditer(r"\S+", p_text))
    if not word_matches:
        return []
    sub_chunks: list[TextChunk] = []
    step = max(1, target_words - overlap_words)
    c_idx = start_chunk_idx
    i = 0
    while i < len(word_matches):
        end_i = min(len(word_matches) - 1, i + target_words - 1)
        rel_start = word_matches[i].start()
        rel_end = word_matches[end_i].end()
        slice_text = p_text[rel_start:rel_end]
        abs_start = p_start + rel_start
        abs_end = p_start + rel_end
        sub_chunks.append(
            TextChunk(
                canonical_book_id=canonical_book_id,
                spine_index=spine_doc.spine_index,
                spine_href=spine_doc.spine_href,
                chapter_title=spine_doc.chapter_title,
                chunk_index=c_idx,
                char_start=abs_start,
                char_end=abs_end,
                chapter_total_chars=chapter_total_chars,
                global_char_start=global_offset_base + abs_start,
                global_char_end=global_offset_base + abs_end,
                book_total_chars=book_total_chars,
                text=slice_text,
            )
        )
        c_idx += 1
        if end_i == len(word_matches) - 1:
            break
        i += step
    return sub_chunks


def parse_epub_file(
    epub_path: str | pathlib.Path,
    canonical_book_id: str | None = None,
    target_words: int = 240,
    overlap_words: int = 40,
) -> ParsedBook:
    """Parse an EPUB into metadata, spine documents, and codepoint-aligned chunks."""
    path = pathlib.Path(epub_path)
    ko_md5 = calculate_koreader_md5(path)
    filename = path.name

    with zipfile.ZipFile(path, "r") as zf:
        container_xml = zf.read("META-INF/container.xml")
        container_root = ET.fromstring(container_xml)
        opf_path = ""
        for elem in container_root.iter():
            if _local_name(elem.tag) == "rootfile":
                opf_path = elem.attrib.get("full-path", "")
                if opf_path:
                    break
        if not opf_path:
            raise ValueError(f"No rootfile in META-INF/container.xml for {path}")

        opf_dir = posixpath.dirname(opf_path)
        opf_root = ET.fromstring(zf.read(opf_path))

        title = ""
        author = ""
        language = ""
        identifiers: list[str] = []
        package_id = ""
        isbn = ""
        asin = ""

        # Parse metadata
        for elem in opf_root.iter():
            lname = _local_name(elem.tag)
            text = (elem.text or "").strip()
            if not text:
                continue
            if lname == "title" and not title:
                title = normalize_whitespace(text)
            elif lname == "creator" and not author:
                author = normalize_whitespace(text)
            elif lname == "language" and not language:
                language = normalize_whitespace(text)
            elif lname == "identifier":
                identifiers.append(text)
                if not package_id:
                    package_id = text
                scheme = ""
                for k, v in elem.attrib.items():
                    if _local_name(k).lower() == "scheme":
                        scheme = v
                cand_isbn, cand_asin = _extract_isbn_or_asin(text, scheme)
                if cand_isbn and not isbn:
                    isbn = cand_isbn
                if cand_asin and not asin:
                    asin = cand_asin

        if not title:
            title = path.stem

        # Parse manifest
        manifest: dict[str, tuple[str, str]] = {}
        for elem in opf_root.iter():
            if _local_name(elem.tag) == "item":
                item_id = elem.attrib.get("id", "")
                href = elem.attrib.get("href", "")
                media_type = elem.attrib.get("media-type", "")
                if item_id and href:
                    full_href = posixpath.normpath(posixpath.join(opf_dir, href)) if opf_dir else posixpath.normpath(href)
                    manifest[item_id] = (full_href, media_type)

        toc_titles = _parse_toc_titles(zf, opf_dir, manifest)

        # Parse spine in exact integer order
        spine_itemrefs: list[str] = []
        for elem in opf_root.iter():
            if _local_name(elem.tag) == "itemref":
                idref = elem.attrib.get("idref", "")
                if idref in manifest:
                    spine_itemrefs.append(idref)

        resolved_id = canonical_book_id or package_id or ko_md5 or filename
        spine_docs: list[SpineDocument] = []
        total_chars = 0

        for spine_idx, idref in enumerate(spine_itemrefs):
            full_href, _media_type = manifest[idref]
            try:
                raw_bytes = zf.read(full_href)
            except KeyError:
                continue
            html_str = raw_bytes.decode("utf-8", errors="replace")
            visible_text, paragraphs = extract_visible_paragraphs(html_str)
            chapter_title = toc_titles.get(full_href, "")
            if not chapter_title and paragraphs:
                first_para = paragraphs[0][2]
                if len(first_para) <= 80:
                    chapter_title = first_para
            if not chapter_title:
                chapter_title = f"Chapter {spine_idx + 1}"

            spine_docs.append(
                SpineDocument(
                    spine_index=spine_idx,
                    spine_href=full_href,
                    chapter_title=chapter_title,
                    visible_text=visible_text,
                    paragraphs=paragraphs,
                )
            )
            total_chars += len(visible_text)

        # Build chunks with both chapter-local and book-global codepoint coordinates
        all_chunks: list[TextChunk] = []
        running_global = 0
        for sdoc in spine_docs:
            ch_chunks = chunk_spine_paragraphs(
                canonical_book_id=resolved_id,
                spine_doc=sdoc,
                global_offset_base=running_global,
                book_total_chars=total_chars,
                target_words=target_words,
                overlap_words=overlap_words,
            )
            all_chunks.extend(ch_chunks)
            running_global += len(sdoc.visible_text)

        return ParsedBook(
            canonical_book_id=resolved_id,
            title=title,
            author=author,
            language=language,
            package_id=package_id,
            identifiers=identifiers,
            isbn=isbn,
            asin=asin,
            ko_md5=ko_md5,
            filename=filename,
            total_chars=total_chars,
            spine_docs=spine_docs,
            chunks=all_chunks,
        )
