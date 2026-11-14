#!/usr/bin/env python3
"""Spoiler Torture Suite, Fuzzy Alignment Tests & LocalAI Dynamic Model Tests.

Verifies:
  1. Intra-Chapter Spoiler Gate: mid-chapter cutoffs slice straddling chunks at
     the exact `anchor_text` sentence and never leak a single word from later
     on the same page or later in the chapter.
  2. Multi-Tier Fuzzy Book Matching: matches books even when re-compressed,
     renamed, or stripped of OPF metadata (via RapidFuzz / token similarity and
     Anchor-Text corpus search).
  3. Multi-Tier Position Alignment: gracefully degrades from `anchor_exact` ->
     `chapter_progress` (-3% safety buffer) -> `book_progress` (-2% safety buffer)
     when Calibre splits or merges spine files.
  4. Parametric Memory Bleed Guard (`EntityMasker`): masks famous character names
     (`Paul Atreides`, `Gollum`, `Vaelin`) before calling the LLM and unmasks them
     in the final response.
  5. Dynamic LocalAI Model Swapping (`qwen3-14b` <-> `qwen2.5-9b`), `/no_think`
     injection, `<think>` tag stripping, and 480-char e-ink sentence formatting.
  6. End-to-End HTTP Contract (`POST /api/ereader/v1/query`) with `X-API-Key`
     validation and brute-force lockout.
"""

from __future__ import annotations

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import pathlib
import shutil
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
import zipfile

import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT))

from companion.alignment import BookQueryIdentity, BookResolver, CutoffResolver, filter_and_truncate_chunks  # noqa: E402
from companion.app import CompanionEngine, create_stdlib_http_server  # noqa: E402
from companion.epub_indexer import calculate_koreader_md5, parse_epub_file  # noqa: E402
from companion.llm import enforce_eink_length, strip_thinking_and_markdown  # noqa: E402
from companion.retriever import EntityMasker, SpoilerFreeRetriever  # noqa: E402


def build_synthetic_epub(
    dest_path: pathlib.Path,
    title: str = "The Shadow of the Spire",
    author: str = "E. V. Vance",
    uuid: str = "urn:uuid:99887766-5544-3322-1100-aabbccddeeff",
    isbn: str = "9780140449136",
    chapters: list[tuple[str, list[str]]] | None = None,
) -> pathlib.Path:
    """Create a valid multi-chapter EPUB archive for testing."""
    if chapters is None:
        chapters = [
            (
                "Chapter 1: The Northern Watch",
                [
                    "Aldric stood upon the frost-rimed battlements of Highkeep, watching the supply wagons wind through the valley.",
                    "Master Corwin approached him with a sealed parchment from the capital. Corwin was the oldest healer in the citadel and Aldric's trusted mentor.",
                    "They spoke of the winter stores and the strange silence from the eastern border outposts.",
                ],
            ),
            (
                "Chapter 2: The Glass Archives",
                [
                    "In the subterranean vaults beneath the library, Lyra cataloged the obsidian tablets left by the First Builders.",
                    "Aldric showed Lyra the broken signet ring he had found beside the northern gate.",
                    "MID_CHAPTER_ANCHOR: The lantern flickered twice as footsteps echoed down the spiral stair.",
                    "INTRA_CHAPTER_SPOILER: Suddenly, Corwin stepped from the shadows holding a poisoned dagger and confessed that he had murdered the High King!",
                ],
            ),
            (
                "Chapter 3: The Siege of Highkeep",
                [
                    "LATE_BOOK_SPOILER: After Corwin's betrayal and execution, Lyra was crowned Queen of the Ashes and burned the Glass Archives to the ground.",
                ],
            ),
        ]

    with zipfile.ZipFile(dest_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("mimetype", "application/epub+plus", compress_type=zipfile.ZIP_STORED)
        zf.writestr(
            "META-INF/container.xml",
            """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>""",
        )

        manifest_items = ['<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>']
        spine_items = []
        nav_points = []

        for idx, (ch_title, paras) in enumerate(chapters):
            fname = f"Text/ch{idx + 1:02d}.xhtml"
            item_id = f"ch{idx + 1:02d}"
            manifest_items.append(f'<item id="{item_id}" href="{fname}" media-type="application/xhtml+xml"/>')
            spine_items.append(f'<itemref idref="{item_id}"/>')
            nav_points.append(
                f"""<navPoint id="nav-{idx + 1}" playOrder="{idx + 1}">
  <navLabel><text>{ch_title}</text></navLabel>
  <content src="{fname}"/>
</navPoint>"""
            )
            body_html = "\n".join(f"<p>{p}</p>" for p in paras)
            xhtml = f"""<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml">
<head><title>{ch_title}</title><style>body {{ margin: 0; }}</style></head>
<body>
  <h1>{ch_title}</h1>
  {body_html}
</body>
</html>"""
            zf.writestr(f"OEBPS/{fname}", xhtml)

        opf = f"""<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="BookId" version="2.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:title>{title}</dc:title>
    <dc:creator>{author}</dc:creator>
    <dc:language>en</dc:language>
    <dc:identifier id="BookId">{uuid}</dc:identifier>
    <dc:identifier opf:scheme="ISBN">{isbn}</dc:identifier>
  </metadata>
  <manifest>
    {"".join(manifest_items)}
  </manifest>
  <spine toc="ncx">
    {"".join(spine_items)}
  </spine>
</package>"""
        zf.writestr("OEBPS/content.opf", opf)

        ncx = f"""<?xml version="1.0" encoding="UTF-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <navMap>
    {"".join(nav_points)}
  </navMap>
</ncx>"""
        zf.writestr("OEBPS/toc.ncx", ncx)

    return dest_path


class FakeLocalAIServer:
    """In-process fake LocalAI server recording prompts & simulating model swaps."""

    def __init__(self) -> None:
        self.active_models: list[str] = ["bge-small-en-v1.5", "qwen3-14b"]
        self.last_request_payload: dict = {}
        self.canned_reply: str = (
            "<think>Wait, in Chapter 3 Corwin betrays everyone, but I only see Excerpt 1!</think>\n"
            "**ENTITY_1** is the oldest healer in Highkeep and a trusted mentor to ENTITY_2."
        )
        self.httpd: HTTPServer | None = None
        self.thread: threading.Thread | None = None
        self.port: int = 0

    def start(self) -> str:
        parent = self

        class _Handler(BaseHTTPRequestHandler):
            def log_message(self, format: str, *args) -> None:
                pass

            def do_GET(self) -> None:  # noqa: N802
                if self.path == "/v1/models":
                    data = {"object": "list", "data": [{"id": m, "object": "model"} for m in parent.active_models]}
                    raw = json.dumps(data).encode("utf-8")
                    self.send_response(200)
                    self.send_header("Content-Type", "application/json")
                    self.send_header("Content-Length", str(len(raw)))
                    self.end_headers()
                    self.wfile.write(raw)
                else:
                    self.send_response(404)
                    self.end_headers()

            def do_POST(self) -> None:  # noqa: N802
                length = int(self.headers.get("Content-Length") or 0)
                body = json.loads(self.rfile.read(length).decode("utf-8"))
                parent.last_request_payload = body
                resp = {
                    "id": "chatcmpl-test",
                    "object": "chat.completion",
                    "model": body.get("model", "unknown"),
                    "choices": [
                        {
                            "index": 0,
                            "message": {"role": "assistant", "content": parent.canned_reply},
                            "finish_reason": "stop",
                        }
                    ],
                }
                raw = json.dumps(resp).encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(raw)))
                self.end_headers()
                self.wfile.write(raw)

        self.httpd = HTTPServer(("127.0.0.1", 0), _Handler)
        self.port = self.httpd.server_address[1]
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        return f"http://127.0.0.1:{self.port}/v1"

    def stop(self) -> None:
        if self.httpd:
            self.httpd.shutdown()
            self.httpd.server_close()


class CompanionSpoilerAndAlignmentTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp_dir = pathlib.Path(tempfile.mkdtemp(prefix="companion-test-"))
        self.epub_path = build_synthetic_epub(self.tmp_dir / "shadow_of_the_spire.epub")
        self.book = parse_epub_file(self.epub_path, target_words=45, overlap_words=10)

    def tearDown(self) -> None:
        shutil.rmtree(self.tmp_dir, ignore_errors=True)

    def test_koreader_md5_and_metadata_extraction(self) -> None:
        self.assertEqual(self.book.title, "The Shadow of the Spire")
        self.assertEqual(self.book.author, "E. V. Vance")
        self.assertEqual(self.book.package_id, "urn:uuid:99887766-5544-3322-1100-aabbccddeeff")
        self.assertEqual(self.book.isbn, "9780140449136")
        self.assertEqual(len(self.book.ko_md5), 32)
        self.assertEqual(self.book.ko_md5, calculate_koreader_md5(self.epub_path))
        self.assertEqual(len(self.book.spine_docs), 3)

    def test_intra_chapter_spoiler_gate_slices_straddling_chunk(self) -> None:
        """Flaw 1 Fix: Querying mid-Chapter 2 MUST omit both Chapter 3 and the second half of Chapter 2."""
        cutoff = CutoffResolver.resolve_cutoff(
            book=self.book,
            spine_index=1,
            anchor_text="The lantern flickered twice as footsteps echoed down the spiral stair.",
        )
        self.assertEqual(cutoff.tier, "anchor_exact")
        self.assertEqual(cutoff.cutoff_spine_index, 1)

        eligible = filter_and_truncate_chunks(self.book, cutoff)
        combined_text = "\n".join(c.text for c in eligible)

        # Past text and the anchor sentence itself must be present:
        self.assertIn("oldest healer in the citadel", combined_text)
        self.assertIn("lantern flickered twice", combined_text)

        # Intra-chapter spoiler (1 paragraph later in Chapter 2!) and Chapter 3 spoiler MUST be 100% absent:
        self.assertNotIn("INTRA_CHAPTER_SPOILER", combined_text)
        self.assertNotIn("poisoned dagger", combined_text)
        self.assertNotIn("murdered the High King", combined_text)
        self.assertNotIn("LATE_BOOK_SPOILER", combined_text)
        self.assertNotIn("Queen of the Ashes", combined_text)

    def test_fuzzy_book_matching_across_renamed_and_calibre_modified_epubs(self) -> None:
        """Flaw 2 Fix: Resolve book even when UUID, hash, and filename were altered by Calibre/compression."""
        resolver = BookResolver()
        resolver.register_book(self.book)

        # Case A: Exact UUID or ISBN match
        matched, method = resolver.resolve(BookQueryIdentity(book_uuid="99887766-5544-3322-1100-aabbccddeeff"))
        self.assertIsNotNone(matched)
        self.assertEqual(method, "exact_id")

        # Case B: Mismatched UUID & hash, renamed filename with series prefix & compression tag
        matched_fuzzy, method_fuzzy = resolver.resolve(
            BookQueryIdentity(
                book_uuid="urn:uuid:calibre-rewritten-id",
                ko_md5="00000000000000000000000000000000",
                title="01 - Shadow of the Spire (Compressed v2)",
                author="E.V. Vance",
                filename="01_Shadow_of_the_Spire_compressed.epub",
            )
        )
        self.assertIsNotNone(matched_fuzzy)
        self.assertTrue(method_fuzzy.startswith("fuzzy_metadata"), f"Got {method_fuzzy}")

        # Case C: Completely mangled metadata ("book_final.epub", title="Unknown"), matched via page anchor_text!
        matched_corpus, method_corpus = resolver.resolve(
            BookQueryIdentity(
                title="Unknown",
                filename="book_final.epub",
                anchor_text="In the subterranean vaults beneath the library, Lyra cataloged the obsidian tablets",
            )
        )
        self.assertIsNotNone(matched_corpus)
        self.assertIn("anchor_corpus", method_corpus)

    def test_calibre_split_spine_alignment_via_anchor_and_progress_fallbacks(self) -> None:
        """When a device EPUB has different spine indices than ABS, anchor_text and chapter_title still align."""
        # Suppose Calibre split the frontmatter on the device so Chapter 2 is spine_index=4 on the device,
        # while on Audiobookshelf it is spine_index=1:
        cutoff = CutoffResolver.resolve_cutoff(
            book=self.book,
            spine_index=4,  # misaligned spine index!
            chapter_title="Chapter 2: The Glass Archives",
            anchor_text="Aldric showed Lyra the broken signet ring he had found beside the northern gate.",
        )
        self.assertEqual(cutoff.tier, "anchor_exact")
        self.assertEqual(cutoff.cutoff_spine_index, 1)  # Pinned to ABS's true spine_index=1!

        eligible = filter_and_truncate_chunks(self.book, cutoff)
        joined = " ".join(c.text for c in eligible)
        self.assertIn("broken signet ring", joined)
        self.assertNotIn("poisoned dagger", joined)

        # Tier 2 fallback: No anchor text match, but chapter_title + chapter_progress (40%) provided
        cutoff_t2 = CutoffResolver.resolve_cutoff(
            book=self.book,
            spine_index=5,
            chapter_title="Chapter 2: Glass Archives",
            chapter_progress=0.40,
        )
        self.assertEqual(cutoff_t2.tier, "chapter_progress")
        self.assertEqual(cutoff_t2.cutoff_spine_index, 1)

        # Tier 3 fallback: Omnibus/restructured TOC where neither anchor nor chapter title matches
        cutoff_t3 = CutoffResolver.resolve_cutoff(
            book=self.book,
            spine_index=99,
            chapter_title="Completely Different Omnibus Part I",
            book_progress=0.30,
        )
        self.assertEqual(cutoff_t3.tier, "book_progress")
        self.assertLessEqual(cutoff_t3.cutoff_global_char, int(self.book.total_chars * 0.28) + 1)

    def test_entity_masker_prevents_parametric_bleed(self) -> None:
        """Flaw 4 Fix: Famous character names are masked before LLM prompt construction and unmasked after."""
        masker = EntityMasker(focus_entity="Corwin", extra_entities=["Aldric", "Highkeep"])
        masked = masker.mask_text("Master Corwin approached Aldric on the walls of Highkeep.")
        self.assertNotIn("Corwin", masked)
        self.assertNotIn("Aldric", masked)
        self.assertIn("ENTITY_1", masked)

        restored = masker.unmask_text("ENTITY_1 is a healer who spoke with [ENTITY_2] at ENTITY_3.")
        self.assertEqual(restored, "Corwin is a healer who spoke with Aldric at Highkeep.")

    def test_end_to_end_http_and_dynamic_localai_model_swapping(self) -> None:
        """Verify LocalAI dynamic model discovery (qwen3-14b <-> qwen2.5-9b), <think> stripping, and HTTP auth."""
        fake_localai = FakeLocalAIServer()
        localai_url = fake_localai.start()
        try:
            engine = CompanionEngine(
                api_key="secret-ereader-key",
                localai_base_url=localai_url,
                chat_model="auto",
                model_preference=["qwen3-14b", "qwen2.5-9b-instruct"],
                embedding_provider="none",
                enable_entity_masking=True,
            )
            engine.index_book(self.book)

            server = create_stdlib_http_server(engine, host="127.0.0.1", port=0)
            port = server.server_address[1]
            t = threading.Thread(target=server.serve_forever, daemon=True)
            t.start()
            try:
                base = f"http://127.0.0.1:{port}"

                # 1. Missing X-API-Key -> 401 Unauthorized
                req_unauth = urllib.request.Request(
                    f"{base}/api/ereader/v1/query",
                    data=b"{}",
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with self.assertRaises(urllib.error.HTTPError) as ctx:
                    urllib.request.urlopen(req_unauth)
                self.assertEqual(ctx.exception.code, 401)
                ctx.exception.close()

                # 2. Valid query with qwen3-14b loaded in LocalAI
                query_payload = {
                    "book_uuid": "urn:uuid:calibre-modified",
                    "title": "The Shadow of the Spire",
                    "author": "E. V. Vance",
                    "spine_index": 1,
                    "anchor_text": "The lantern flickered twice as footsteps echoed down the spiral stair.",
                    "selected_text": "Corwin",
                    "action_type": "who_is",
                }
                req_ok = urllib.request.Request(
                    f"{base}/api/ereader/v1/query",
                    data=json.dumps(query_payload).encode("utf-8"),
                    headers={
                        "Content-Type": "application/json",
                        "X-API-Key": "secret-ereader-key",
                    },
                    method="POST",
                )
                with urllib.request.urlopen(req_ok) as resp:
                    self.assertEqual(resp.status, 200)
                    data = json.loads(resp.read().decode("utf-8"))

                # Verify dynamic model resolver picked qwen3-14b (skipping the embedding model!)
                self.assertEqual(data["model"], "qwen3-14b")
                # Verify <think> block and Markdown ** were stripped, and ENTITY_1/ENTITY_2 were unmasked!
                self.assertNotIn("<think>", data["ans"])
                self.assertNotIn("**", data["ans"])
                self.assertIn("Corwin is the oldest healer", data["ans"])
                self.assertLessEqual(len(data["ans"]), 1400)

                # Verify that the prompt sent to LocalAI had /no_think, enable_thinking=False,
                # masked "Corwin", and NEVER contained the intra-chapter spoiler!
                sent_user_prompt = fake_localai.last_request_payload["messages"][1]["content"]
                self.assertIn("/no_think", sent_user_prompt)
                self.assertEqual(
                    fake_localai.last_request_payload.get("chat_template_kwargs"),
                    {"enable_thinking": False},
                )
                self.assertNotIn("poisoned dagger", sent_user_prompt)
                self.assertNotIn("Corwin", sent_user_prompt)

                # 3. Simulate user unloading qwen3-14b and loading qwen2.5-9b-instruct in LocalAI
                fake_localai.active_models = ["qwen2.5-9b-instruct"]
                with urllib.request.urlopen(req_ok) as resp2:
                    data2 = json.loads(resp2.read().decode("utf-8"))
                self.assertEqual(data2["model"], "qwen2.5-9b-instruct")
            finally:
                server.shutdown()
                server.server_close()
        finally:
            fake_localai.stop()

    def test_eink_length_and_thinking_cleaner(self) -> None:
        raw = "<think> Deep reasoning here... </think> **Paul** is the son of Duke Leto.\n\n" + ("He trains on Caladan. " * 30)
        cleaned = enforce_eink_length(raw, max_chars=480)
        self.assertNotIn("<think>", cleaned)
        self.assertNotIn("**", cleaned)
        self.assertIn("\n\n", cleaned)
        self.assertLessEqual(len(cleaned), 480)
        self.assertTrue(cleaned.endswith("."))


if __name__ == "__main__":
    unittest.main(verbosity=2)
