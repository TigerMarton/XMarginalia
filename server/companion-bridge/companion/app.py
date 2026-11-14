"""Core orchestrator engine and HTTP handlers for `companion-bridge`.

Exposes:
  * `POST /api/ereader/v1/query` — Main spoiler-free AI companion query endpoint
  * `POST /api/ereader/v1/sync`  — Trigger library rescan from `/abs_books` & ABS API
  * `GET  /healthz`              — Container healthcheck
"""

from __future__ import annotations

import asyncio
import dataclasses
import hmac
import sys
import threading
import time
from collections import OrderedDict
from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import os
import pathlib
from typing import Any

from companion.abs_sync import AudiobookshelfClient, iter_epub_paths
from companion.alignment import BookQueryIdentity, BookResolver, CutoffResolver
from companion.embeddings import build_embedder
from companion.epub_indexer import ParsedBook, parse_epub_file
from companion.llm import LocalAIClient
from companion.ratelimit import Lockout, Window
from companion.retriever import SpoilerFreeRetriever


class CompanionEngine:
    """Self-contained Anti-Spoiler RAG Engine."""

    def __init__(
        self,
        api_key: str = "",
        localai_base_url: str = "http://127.0.0.1:8080/v1",
        chat_model: str = "auto",
        model_preference: list[str] | None = None,
        embedding_provider: str = "fastembed_cpu",
        embedding_model: str = "BAAI/bge-small-en-v1.5",
        abs_books_mount: str = "",
        abs_base_url: str = "",
        abs_api_token: str = "",
        enable_entity_masking: bool = True,
    ) -> None:
        self.api_key = api_key
        self.resolver = BookResolver()
        embed_fn = build_embedder(
            provider=embedding_provider,
            model_name=embedding_model,
            localai_base_url=localai_base_url,
        )
        self.retriever = SpoilerFreeRetriever(embed_fn=embed_fn)
        self.llm = LocalAIClient(
            base_url=localai_base_url,
            chat_model=chat_model,
            model_preference=model_preference,
            enable_entity_masking=enable_entity_masking,
        )
        self.abs_books_mount = abs_books_mount
        self.abs_client = (
            AudiobookshelfClient(abs_base_url, abs_api_token)
            if (abs_base_url and abs_api_token)
            else None
        )
        self.rate_window = Window(limit=30, per_s=60.0)
        self.auth_lockout = Lockout()
        # Library is indexed as lightweight metadata stubs (no chunk text);
        # full books are parsed on demand and kept in a small LRU cache so a
        # huge library fits in memory and never blocks the server.
        self.paths: dict[str, pathlib.Path] = {}
        self._full_cache: OrderedDict[str, ParsedBook] = OrderedDict()
        self._cache_max = int(os.environ.get("COMPANION_BOOK_CACHE", "3"))
        self._cache_lock = threading.Lock()
        self._embedding: set[str] = set()
        self.abs_download_dir = os.environ.get("ABS_DOWNLOAD_DIR", "/data/abs_cache")
        self.sync_lock = threading.Lock()
        self.sync_state: dict[str, Any] = {"running": False, "scanned": 0, "indexed": 0, "failed": 0, "last_error": ""}

    @classmethod
    def from_env(cls) -> CompanionEngine:
        prefs_raw = os.environ.get("LOCALAI_MODEL_PREFERENCE", "qwen3-14b,qwen2.5-9b-instruct,qwen3-8b")
        prefs = [p.strip() for p in prefs_raw.split(",") if p.strip()]
        mask_flag = os.environ.get("COMPANION_ENTITY_MASKING", "1").strip().lower() not in ("0", "false", "no")
        engine = cls(
            api_key=os.environ.get("COMPANION_API_KEY", ""),
            localai_base_url=os.environ.get("LOCALAI_BASE_URL", "http://localai:8080/v1"),
            chat_model=os.environ.get("LOCALAI_CHAT_MODEL", "auto"),
            model_preference=prefs,
            embedding_provider=os.environ.get("EMBEDDING_PROVIDER", "fastembed_cpu"),
            embedding_model=os.environ.get("EMBEDDING_MODEL", "BAAI/bge-small-en-v1.5"),
            abs_books_mount=os.environ.get("ABS_BOOKS_MOUNT", ""),
            abs_base_url=os.environ.get("ABS_BASE_URL", ""),
            abs_api_token=os.environ.get("ABS_API_TOKEN", ""),
            enable_entity_masking=mask_flag,
        )
        engine.start_background_sync()
        return engine

    def index_book(self, book: ParsedBook, path: pathlib.Path | None = None) -> None:
        """Register a book. With `path`, only a metadata stub is kept in RAM."""
        if path is None:
            self.resolver.register_book(book)
            self.retriever.index_book_vectors(book)
            return
        self.paths[book.canonical_book_id] = pathlib.Path(path)
        self.resolver.register_book(dataclasses.replace(book, spine_docs=[], chunks=[]))

    def _load_full(self, stub: ParsedBook) -> ParsedBook:
        path = self.paths.get(stub.canonical_book_id)
        if path is None or stub.chunks:
            return stub
        with self._cache_lock:
            hit = self._full_cache.get(stub.canonical_book_id)
            if hit is not None:
                self._full_cache.move_to_end(stub.canonical_book_id)
                return hit
        full = parse_epub_file(path, canonical_book_id=stub.canonical_book_id)
        with self._cache_lock:
            self._full_cache[stub.canonical_book_id] = full
            while len(self._full_cache) > self._cache_max:
                old_id, _ = self._full_cache.popitem(last=False)
                self.retriever.drop_book_vectors(old_id)
        self._ensure_vectors(full)
        return full

    def _ensure_vectors(self, full: ParsedBook) -> None:
        """Embed a book in the background; queries use BM25 until it finishes."""
        bid = full.canonical_book_id
        with self._cache_lock:
            if self.retriever.embed_fn is None or self.retriever.has_vectors(bid) or bid in self._embedding:
                return
            self._embedding.add(bid)

        def _run() -> None:
            try:
                self.retriever.index_book_vectors(full)
                with self._cache_lock:
                    if bid not in self._full_cache:
                        self.retriever.drop_book_vectors(bid)
            except Exception as exc:
                print(f"[companion] embedding failed for {full.title!r}: {exc}", file=sys.stderr, flush=True)
            finally:
                with self._cache_lock:
                    self._embedding.discard(bid)

        threading.Thread(target=_run, daemon=True).start()

    def index_epub_path(self, epub_path: str | pathlib.Path, canonical_book_id: str | None = None) -> ParsedBook:
        book = parse_epub_file(epub_path, canonical_book_id=canonical_book_id)
        self.index_book(book)
        return book

    def sync_library(self) -> int:
        """Scan the library once, registering metadata stubs. Returns books indexed."""
        if not self.sync_lock.acquire(blocking=False):
            return len(self.resolver.books)
        st = self.sync_state
        st.update(running=True, scanned=0, indexed=0, failed=0, last_error="", started=time.time())
        try:
            if self.abs_books_mount and pathlib.Path(self.abs_books_mount).is_dir():
                for path in iter_epub_paths(self.abs_books_mount):
                    st["scanned"] += 1
                    try:
                        self.index_book(parse_epub_file(path), path=path)
                        st["indexed"] += 1
                    except Exception as exc:
                        st["failed"] += 1
                        st["last_error"] = f"{path.name}: {exc!r}"
                    if st["scanned"] % 50 == 0:
                        print(f"[companion] sync progress: {st['indexed']}/{st['scanned']} ({st['failed']} failed)", file=sys.stderr, flush=True)
            elif self.abs_books_mount:
                st["last_error"] = f"ABS_BOOKS_MOUNT {self.abs_books_mount} is not a directory"
            if self.abs_client is not None:
                try:
                    for item_id, path, md in self.abs_client.iter_epub_items(self.abs_download_dir):
                        st["scanned"] += 1
                        try:
                            b = parse_epub_file(path, canonical_book_id=item_id)
                            if md.get("title"):
                                b.title = str(md["title"])
                            if md.get("authorName"):
                                b.author = str(md["authorName"])
                            if md.get("isbn") and not b.isbn:
                                b.isbn = str(md["isbn"])
                            if md.get("asin") and not b.asin:
                                b.asin = str(md["asin"])
                            self.index_book(b, path=path)
                            st["indexed"] += 1
                        except Exception as exc:
                            st["failed"] += 1
                            st["last_error"] = f"{path.name}: {exc!r}"
                except Exception as exc:
                    st["last_error"] = f"ABS API: {exc!r}"
            print(f"[companion] sync done: {st['indexed']} indexed, {st['failed']} failed, {st['last_error']}", file=sys.stderr, flush=True)
            return st["indexed"]
        finally:
            st["running"] = False
            self.sync_lock.release()

    def start_background_sync(self) -> bool:
        if self.sync_state.get("running"):
            return False
        threading.Thread(target=self.sync_library, daemon=True).start()
        return True

    def verify_api_key(self, provided_key: str, client_ip: str = "127.0.0.1") -> tuple[bool, int, str]:
        """Return `(ok, status_code, error_message)`."""
        if self.auth_lockout.locked_for(client_ip) > 0:
            return False, 429, "Too many failed authentication attempts. Please wait."
        if not self.rate_window.allow(client_ip):
            return False, 429, "Rate limit exceeded. Please wait a moment."
        if not self.api_key:
            return True, 200, ""
        if not provided_key or not hmac.compare_digest(provided_key.encode("utf-8"), self.api_key.encode("utf-8")):
            self.auth_lockout.record_failure(client_ip)
            return False, 401, "Invalid or missing X-API-Key."
        self.auth_lockout.record_success(client_ip)
        return True, 200, ""

    def handle_query(self, payload: dict[str, Any]) -> tuple[int, dict[str, Any]]:
        """Process an e-reader companion request and return `(http_status, json_body)`."""
        identity = BookQueryIdentity(
            book_uuid=str(payload.get("book_uuid") or payload.get("package_id") or ""),
            isbn=str(payload.get("isbn") or ""),
            asin=str(payload.get("asin") or ""),
            ko_md5=str(payload.get("ko_md5") or ""),
            title=str(payload.get("title") or ""),
            author=str(payload.get("author") or ""),
            filename=str(payload.get("filename") or ""),
            anchor_text=str(payload.get("anchor_text") or ""),
            selected_text=str(payload.get("selected_text") or ""),
        )

        book, match_method = self.resolver.resolve(identity)
        if book is None:
            return 404, {"error": "Book not found on companion server. Sync Audiobookshelf first."}

        try:
            book = self._load_full(book)
        except Exception as exc:
            return 500, {"error": f"Could not open book file: {exc}"}

        spine_index = int(payload.get("spine_index") or 0)
        chapter_title = str(payload.get("chapter_title") or "")
        vis_raw = payload.get("visible_offset")
        visible_offset = int(vis_raw) if vis_raw is not None and int(vis_raw) >= 0 else None

        ch_prog_raw = payload.get("chapter_progress")
        chapter_progress = float(ch_prog_raw) if ch_prog_raw is not None else None

        bk_prog_raw = payload.get("book_progress")
        if bk_prog_raw is None and payload.get("progress_bps") is not None:
            bk_prog_raw = float(payload["progress_bps"]) / 10000.0
        book_progress = float(bk_prog_raw) if bk_prog_raw is not None else None

        cutoff = CutoffResolver.resolve_cutoff(
            book=book,
            spine_index=spine_index,
            chapter_title=chapter_title,
            visible_offset=visible_offset,
            chapter_progress=chapter_progress,
            book_progress=book_progress,
            anchor_text=identity.anchor_text,
        )

        action_type = str(payload.get("action_type") or "who_is")
        ret = self.retriever.retrieve(
            book=book,
            cutoff=cutoff,
            action_type=action_type,
            selected_text=identity.selected_text,
            anchor_text=identity.anchor_text,
            top_k=5,
        )

        try:
            ans, model_used = self.llm.generate_companion_answer(
                action_type=action_type,
                excerpts=ret.selected_chunks,
                selected_text=identity.selected_text,
                anchor_text=identity.anchor_text,
                book_title=book.title,
                book_author=book.author,
                progress_label=cutoff.progress_label,
            )
        except Exception as exc:
            print(f"[companion] LocalAI query failed: {exc!r}", file=sys.stderr, flush=True)
            return 502, {"error": "LocalAI model is currently unreachable or loading."}

        return 200, {
            "ans": ans,
            "pos": cutoff.progress_label,
            "model": model_used,
            "match": match_method,
            "tier": cutoff.tier,
        }


# Optional FastAPI app wrapper (used when running under uvicorn in Docker)
try:
    from fastapi import FastAPI, Request
    from fastapi.responses import JSONResponse

    app = FastAPI(title="XMarginalia Anti-Spoiler Companion Bridge", docs_url=None, redoc_url=None)
    _ENGINE: CompanionEngine | None = None

    def get_engine() -> CompanionEngine:
        global _ENGINE
        if _ENGINE is None:
            _ENGINE = CompanionEngine.from_env()
        return _ENGINE

    @app.get("/healthz")
    async def healthz() -> dict[str, Any]:
        eng = get_engine()
        return {"status": "ok", "indexed_books": len(eng.resolver.books), "syncing": bool(eng.sync_state.get("running"))}

    @app.post("/api/ereader/v1/query")
    @app.post("/v1/companion/query")
    async def query_endpoint(request: Request) -> JSONResponse:
        eng = get_engine()
        client_ip = request.client.host if request.client else "127.0.0.1"
        api_key = request.headers.get("X-API-Key", "")
        ok_auth, status_auth, err_msg = eng.verify_api_key(api_key, client_ip=client_ip)
        if not ok_auth:
            return JSONResponse({"error": err_msg}, status_code=status_auth)
        try:
            payload = await request.json()
        except Exception:
            return JSONResponse({"error": "Invalid JSON request body."}, status_code=400)
        status, body = await asyncio.to_thread(eng.handle_query, payload)
        return JSONResponse(body, status_code=status)

    @app.post("/api/ereader/v1/sync")
    @app.post("/v1/companion/sync")
    async def sync_endpoint(request: Request) -> JSONResponse:
        eng = get_engine()
        client_ip = request.client.host if request.client else "127.0.0.1"
        api_key = request.headers.get("X-API-Key", "")
        ok_auth, status_auth, err_msg = eng.verify_api_key(api_key, client_ip=client_ip)
        if not ok_auth:
            return JSONResponse({"error": err_msg}, status_code=status_auth)
        started = eng.start_background_sync()
        return JSONResponse(
            {"status": "started" if started else "already_running", "total_books": len(eng.resolver.books), **eng.sync_state},
            status_code=202,
        )

    @app.get("/api/ereader/v1/sync/status")
    async def sync_status(request: Request) -> JSONResponse:
        eng = get_engine()
        client_ip = request.client.host if request.client else "127.0.0.1"
        ok_auth, status_auth, err_msg = eng.verify_api_key(request.headers.get("X-API-Key", ""), client_ip=client_ip)
        if not ok_auth:
            return JSONResponse({"error": err_msg}, status_code=status_auth)
        return JSONResponse({"total_books": len(eng.resolver.books), **eng.sync_state})

except ImportError:
    app = None  # type: ignore[assignment]


def create_stdlib_http_server(engine: CompanionEngine, host: str = "127.0.0.1", port: int = 8095) -> HTTPServer:
    """Zero-dependency HTTP server for local simulator & integration testing."""

    class _Handler(BaseHTTPRequestHandler):
        def log_message(self, format: str, *args: Any) -> None:
            pass

        def _send_json(self, status: int, body: dict[str, Any]) -> None:
            raw = json.dumps(body).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def do_GET(self) -> None:  # noqa: N802
            if self.path == "/healthz":
                self._send_json(200, {"status": "ok", "indexed_books": len(engine.resolver.books)})
            else:
                self._send_json(404, {"error": "Not found"})

        def do_POST(self) -> None:  # noqa: N802
            client_ip = self.client_address[0] if self.client_address else "127.0.0.1"
            api_key = self.headers.get("X-API-Key", "")
            ok_auth, status_auth, err_msg = engine.verify_api_key(api_key, client_ip=client_ip)
            if not ok_auth:
                self._send_json(status_auth, {"error": err_msg})
                return
            length = int(self.headers.get("Content-Length") or 0)
            raw_body = self.rfile.read(length) if length > 0 else b"{}"
            if self.path in ("/api/ereader/v1/query", "/v1/companion/query"):
                try:
                    payload = json.loads(raw_body.decode("utf-8"))
                except Exception:
                    self._send_json(400, {"error": "Invalid JSON request body."})
                    return
                status, resp = engine.handle_query(payload)
                self._send_json(status, resp)
            elif self.path in ("/api/ereader/v1/sync", "/v1/companion/sync"):
                synced = engine.sync_library()
                self._send_json(200, {"status": "ok", "synced": synced, "total_books": len(engine.resolver.books)})
            else:
                self._send_json(404, {"error": "Not found"})

    return HTTPServer((host, port), _Handler)
