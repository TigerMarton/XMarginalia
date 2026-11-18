# AI Companion (`companion-bridge`)

XMarginalia includes an optional, spoiler-free **AI Companion** inside the EPUB reader (`src/apps_local/companion/`) backed by a self-hosted Python bridge server (`server/companion-bridge/`). Every query is strictly gated to the text you have already read—nothing past your current page is ever retrieved or sent to the language model.

---

## 1. How the AI Functions Work

### In-Reader Actions

* **Word / Passage Selection (`ClipSelectionActivity` -> `EpubReaderActivity`)**:
  * **Who is this? (`who_is`)**: Summarizes who the selected character is, how they were introduced, and their active role/situation up to your current page.
  * **What is this? (`what_is`)**: Explains what a selected term, place, or concept is and why it matters at your current point in the story.
  * **Explain (`explain`)**: Clarifies the immediate passage on the page and connects it to earlier events or motives revealed so far.
* **Reader Menu (`EpubReaderMenuActivity` -> `EpubReaderActivity`)**:
  * **Where was I? (`recap_story`)**: Recaps the main plot arcs and where things stand right now so you can pick a book back up.
  * **Recap chapter so far (`recap_chapter`)**: Summarizes the current chapter chronologically from its opening up to your current page.

### Device Flow & Memory Discipline

1. When you trigger an AI action, the reader captures your book identifiers (`abs_item_id` from `<book>.meta.json` when downloaded via the Audiobookshelf plugin, plus `book_uuid`, `isbn`, `asin`, `ko_md5`, `title`, `author`, `filename`), your exact reading position (`spine_index`, `chapter_title`, `visible_offset`, `chapter_progress`, `book_progress`), and a short `anchor_text` snippet from the visible page.
2. Config is loaded from `/.crosspoint/companion.json` (or optionally `companion_url` / `companion_api_key` in the Audiobookshelf plugin's `config.json` when `companion.json` is absent). Before waking Wi-Fi or starting TLS, `EpubReaderActivity::runPendingCompanionQuery()` stops the BLE page turner and releases rebuildable reader state (`section`, `epub`, `CLIPPINGS`, overlay page, image render cache, and SD font caches) to maximize contiguous internal SRAM.
3. `CompanionClient::query()` connects to your saved Wi-Fi network, sends `POST /api/ereader/v1/query` over HTTPS (or LAN HTTP if configured) with `X-API-Key`, and tears down the Wi-Fi radio when finished.
4. `EpubReaderActivity` reloads the EPUB from its SD cache and displays the response in `DictionaryDefinitionActivity`, paginated and formatted with your current reader font (`SETTINGS.getReaderFontId()`) and line spacing (`SETTINGS.getReaderLineCompression()`).

### Server Pipeline (`companion-bridge`)

1. **Multi-Tier Book Resolution (`BookResolver`)**: Matches the e-reader's request against your library using `abs_item_id`, `<dc:identifier>` UUID, ISBN, ASIN, or `ko_md5`, falling back to fuzzy normalized `title + author` / filename matching (`>= 82%` via RapidFuzz), and finally full-corpus `anchor_text` matching.
2. **Lazy Indexing & Low-RAM Caching**: On startup and `/api/ereader/v1/sync`, the server scans EPUBs into lightweight metadata stubs (without keeping every book's full text in RAM). Full EPUB chunks are parsed on demand and held in a small LRU cache (`COMPANION_BOOK_CACHE`, default `3`).
3. **Strict Intra-Chapter Spoiler Gate (`CutoffResolver`)**:
   * **Tier 1 (`anchor_exact`)**: Finds `anchor_text` in the indexed chunks and truncates any straddling chunk immediately after that passage.
   * **Tier 2 (`chapter_progress`)**: Matches `chapter_title` / `spine_index` and scales by `chapter_progress` minus a `3%` safety margin.
   * **Tier 3 (`book_progress`)**: Maps whole-book progress minus a `2%` safety margin against cumulative book text.
4. **Book-Focused Retrieval (`SpoilerFreeRetriever`)**:
   * `who_is` / `what_is`: Combines the character/term's **first mention**, **most recent mentions** before the cutoff, the **current page chunk**, and top hybrid BM25 + dense vector matches (`fastembed_cpu` using `BAAI/bge-small-en-v1.5` on CPU RAM by default, or `none` for pure BM25).
   * `recap_chapter`: Samples evenly across the current chapter up to the cutoff plus the trailing chunks at your current page.
   * `recap_story`: Samples representative chunks across earlier spine chapters plus recent progression up to your current page.
   * `explain`: Combines the immediate local scene (last 3 chunks) with top earlier matches.
5. **Optional Entity Masking & LLM Inference (`EntityMasker`, `LocalAIClient`)**:
   * Proper nouns can be temporarily replaced with `[ENTITY_N]` placeholders (`COMPANION_ENTITY_MASKING=1`) before calling the LLM and restored afterward to reduce pre-trained knowledge bleed on famous books.
   * Calls any OpenAI-compatible `/v1/chat/completions` endpoint (such as LocalAI). When `LOCALAI_CHAT_MODEL=auto`, it queries `/v1/models` and selects the first available model from `LOCALAI_MODEL_PREFERENCE`.

---

## 2. Setting Up the Server

### Prerequisites

* Docker and Docker Compose (or Python 3.13+).
* An OpenAI-compatible inference backend (such as **LocalAI**, **llama.cpp**, or **vLLM**) reachable from the container.
* Your EPUB library directory on the server host, and/or an **Audiobookshelf** instance.

### Step 1: Run `companion-bridge` with Docker Compose

Add an `ereader-companion` service alongside your LocalAI (or OpenAI-compatible) service in `docker-compose.yml`:

```yaml
services:
  ereader-companion:
    build: ./server/companion-bridge
    container_name: ereader-companion
    restart: unless-stopped
    ports:
      - "127.0.0.1:8095:8095" # Bind to localhost if putting Caddy/reverse proxy in front
    environment:
      COMPANION_API_KEY: "replace-with-a-long-random-secret"
      LOCALAI_BASE_URL: "http://localai:8080/v1"
      LOCALAI_CHAT_MODEL: "auto"
      LOCALAI_MODEL_PREFERENCE: "qwen3-14b,qwen2.5-9b-instruct,qwen3-8b"
      EMBEDDING_PROVIDER: "fastembed_cpu" # or "none" for zero-RAM BM25-only retrieval
      EMBEDDING_MODEL: "BAAI/bge-small-en-v1.5"
      ABS_BOOKS_MOUNT: "/abs_books"
      # Optional: also sync metadata/books from Audiobookshelf
      # ABS_BASE_URL: "http://audiobookshelf:13378"
      # ABS_API_TOKEN: "your-audiobookshelf-token"
    volumes:
      - /path/to/your/books:/abs_books:ro
      - companion_data:/data

volumes:
  companion_data:
```

> [!TIP]
> You can also run `./server/companion-bridge/scripts/generate-deploy-config.sh` to generate a `.env` file, a `docker-compose.yml` snippet, and a Caddy snippet interactively.

### Step 2: Optional Caddy Reverse Proxy (Recommended for TLS)

If you want HTTPS on your LAN or use **Caddy** as a reverse proxy (for example, alongside an SSO gateway like Authelia), place the `/api/ereader/*` handler **before** any interactive `forward_auth` block so the e-reader can authenticate directly via `X-API-Key` (see [`server/companion-bridge/deploy/Caddyfile.snippet`](../server/companion-bridge/deploy/Caddyfile.snippet)):

```caddyfile
companion.example.com {
    handle /api/ereader/* {
        @valid_ereader_key header X-API-Key "{$COMPANION_API_KEY}"
        handle @valid_ereader_key {
            reverse_proxy ereader-companion:8095
        }
        header Content-Type "application/json"
        respond `{"error":"Unauthorized"}` 401
    }

    handle {
        # Your existing web / SSO rules
        reverse_proxy your-existing-service:8080
    }
}
```

### Step 3: Configure the E-Reader (`/.crosspoint/companion.json`)

Create `/.crosspoint/companion.json` on your e-reader's SD card:

```json
{
  "host": "companion.example.com",
  "api_key": "replace-with-a-long-random-secret",
  "enabled": true
}
```

For plain HTTP on a trusted local network (e.g., `192.168.1.50:8095`), set `"use_http": true`:

```json
{
  "host": "192.168.1.50:8095",
  "api_key": "replace-with-a-long-random-secret",
  "use_http": true,
  "enabled": true
}
```

### Step 4: Verify & Sync

```bash
# Health check
curl -s http://127.0.0.1:8095/healthz

# Trigger background library scan
curl -s -X POST http://127.0.0.1:8095/api/ereader/v1/sync \
  -H "X-API-Key: replace-with-a-long-random-secret"
```

---

## 3. Security Warning: Exposing the Server Externally

> [!WARNING]
> **Opening `companion-bridge` to the public internet is risky.**
>
> Even when gated behind an `X-API-Key`, exposing a self-hosted bridge and local LLM backend to the open internet increases your attack surface:
> - **Resource exhaustion / GPU abuse**: A leaked key or unauthenticated edge path can trigger expensive LLM inference or EPUB parsing.
> - **Library & reading privacy**: The server indexes your personal book collection and processes passages from what you are reading.
> - **Software vulnerabilities**: FastAPI, Uvicorn, Python EPUB parsers, and upstream inference backends are not hardened as internet-facing multi-tenant gateways.
>
> **Recommended deployment**: Keep `companion-bridge` on your **private LAN** or behind a **private VPN / mesh network** (such as WireGuard or Tailscale) rather than port-forwarding it publicly. If you must expose it through Caddy on a public domain, always enforce **HTTPS**, use a high-entropy random `COMPANION_API_KEY` (never reuse passwords), restrict source IPs/firewall rules where possible, and keep rate limiting enabled.
