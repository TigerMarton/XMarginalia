"""Audiobookshelf (ABS) Library Sync & Local Folder Scanner.

Supports two ways to ingest EPUBs into the companion index:
  1. Direct folder scan (`ABS_BOOKS_MOUNT=/abs_books`), which walks a read-only
     bind mount of the Audiobookshelf library folder and indexes every `.epub`
     file (zero HTTP overhead);
  2. Audiobookshelf REST API sync (`ABS_BASE_URL` + `ABS_API_TOKEN`), which
     queries `/api/libraries` and downloads `.epub` items on demand.
"""

from __future__ import annotations

import json
import os
import pathlib
import tempfile
import urllib.request

from companion.epub_indexer import ParsedBook, parse_epub_file


def iter_epub_paths(root_dir: str | pathlib.Path):
    """Yield every `.epub` path under `root_dir`, recursively."""
    root = pathlib.Path(root_dir)
    if not root.is_dir():
        return
    for dirpath, _dirnames, filenames in os.walk(root, followlinks=True):
        for fname in sorted(filenames):
            if fname.lower().endswith(".epub") and not fname.startswith("."):
                yield pathlib.Path(dirpath) / fname


def scan_epub_directory(
    root_dir: str | pathlib.Path,
    target_words: int = 240,
    overlap_words: int = 40,
) -> list[ParsedBook]:
    """Recursively scan a directory for `.epub` files and parse them."""
    root = pathlib.Path(root_dir)
    if not root.is_dir():
        return []
    books: list[ParsedBook] = []
    for dirpath, _dirnames, filenames in os.walk(root):
        for fname in sorted(filenames):
            if not fname.lower().endswith(".epub"):
                continue
            full_path = pathlib.Path(dirpath) / fname
            try:
                parsed = parse_epub_file(
                    full_path,
                    target_words=target_words,
                    overlap_words=overlap_words,
                )
                books.append(parsed)
            except Exception:
                continue
    return books


class AudiobookshelfClient:
    """Fetches EPUB metadata and files from an Audiobookshelf server."""

    def __init__(self, base_url: str, api_token: str, timeout_s: float = 15.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.api_token = api_token.strip()
        self.timeout_s = timeout_s

    def _get_json(self, path: str) -> dict:
        req = urllib.request.Request(
            f"{self.base_url}{path}",
            headers={"Authorization": f"Bearer {self.api_token}", "Accept": "application/json"},
            method="GET",
        )
        with urllib.request.urlopen(req, timeout=self.timeout_s) as resp:
            return json.loads(resp.read().decode("utf-8"))

    def list_library_ids(self) -> list[str]:
        if not self.base_url or not self.api_token:
            return []
        data = self._get_json("/api/libraries")
        libs = data.get("libraries") or []
        return [str(lib["id"]) for lib in libs if isinstance(lib, dict) and lib.get("id")]

    def iter_epub_items(self, download_dir: str | pathlib.Path):
        """Yield `(item_id, local_path, metadata_dict)` downloading as needed."""
        if not self.base_url or not self.api_token:
            return
        tmp_root = pathlib.Path(download_dir)
        tmp_root.mkdir(parents=True, exist_ok=True)
        for lib_id in self.list_library_ids():
            items_data = self._get_json(f"/api/libraries/{lib_id}/items?limit=0")
            for item in items_data.get("results") or []:
                item_id = item.get("id")
                media = item.get("media") or {}
                ebook_file = media.get("ebookFile") or {}
                ino = ebook_file.get("ino")
                ext = (ebook_file.get("metadata") or {}).get("ext", "").lower()
                if not item_id or not ino or ext != ".epub":
                    continue
                dest = tmp_root / f"{item_id}.epub"
                if not dest.exists():
                    req = urllib.request.Request(
                        f"{self.base_url}/api/items/{item_id}/file/{ino}/download",
                        headers={"Authorization": f"Bearer {self.api_token}"},
                        method="GET",
                    )
                    try:
                        with urllib.request.urlopen(req, timeout=60.0) as resp:
                            dest.write_bytes(resp.read())
                    except Exception:
                        continue
                yield str(item_id), dest, (media.get("metadata") or {})

    def sync_library_epubs(self, download_dir: str | pathlib.Path | None = None) -> list[ParsedBook]:
        """Download and parse EPUB items across all Audiobookshelf libraries."""
        if not self.base_url or not self.api_token:
            return []
        books: list[ParsedBook] = []
        tmp_root = pathlib.Path(download_dir) if download_dir else pathlib.Path(tempfile.mkdtemp(prefix="abs-epubs-"))
        tmp_root.mkdir(parents=True, exist_ok=True)

        for lib_id in self.list_library_ids():
            items_data = self._get_json(f"/api/libraries/{lib_id}/items?limit=0")
            for item in items_data.get("results") or []:
                item_id = item.get("id")
                media = item.get("media") or {}
                ebook_file = media.get("ebookFile") or {}
                ino = ebook_file.get("ino")
                ext = (ebook_file.get("metadata") or {}).get("ext", "").lower()
                if not item_id or not ino or ext != ".epub":
                    continue
                dest = tmp_root / f"{item_id}.epub"
                if not dest.exists():
                    req = urllib.request.Request(
                        f"{self.base_url}/api/items/{item_id}/file/{ino}/download",
                        headers={"Authorization": f"Bearer {self.api_token}"},
                        method="GET",
                    )
                    try:
                        with urllib.request.urlopen(req, timeout=self.timeout_s) as resp:
                            dest.write_bytes(resp.read())
                    except Exception:
                        continue
                try:
                    parsed = parse_epub_file(dest, canonical_book_id=str(item_id))
                    md = media.get("metadata") or {}
                    if md.get("title"):
                        parsed.title = str(md["title"])
                    if md.get("authorName"):
                        parsed.author = str(md["authorName"])
                    if md.get("isbn") and not parsed.isbn:
                        parsed.isbn = str(md["isbn"])
                    if md.get("asin") and not parsed.asin:
                        parsed.asin = str(md["asin"])
                    books.append(parsed)
                except Exception:
                    continue
        return books
