# Audiobookshelf Plugin

Browse your Audiobookshelf libraries, download EPUB books directly to your SD card, and sync reading progress in both directions without a separate OPDS or KOReader server.

## Installation

1. On the device, enable **Settings -> Connectivity -> Plugins**.
2. Copy the `audiobookshelf` folder onto your SD card at either:
   - `/plugins/audiobookshelf/`
   - `/.crosspoint/plugins/audiobookshelf/`
3. Start **File Transfer** (**Settings -> File Transfer -> Local Web UI** or USB/Wi-Fi web portal) and open **http://crosspoint.local/plugins** (or your device's IP `/plugins`).
4. Enter your Audiobookshelf **Server URL** (e.g. `https://abs.example.com`) and either sign in with your **Username & Password** (optionally saved device-bound obfuscated on SD as `password_obf`, never exposed back over HTTP) or paste an **Audiobookshelf API Token**, then click **Save & Sign In** and **Test Connection**.

## Features

- **Direct Library Browsing & Search**: Select **Home -> Plugins -> Audiobookshelf** to open **Continue Reading**, **Recently Added**, **By Title (A-Z)**, **By Author (A-Z)**, **Search Books**, or **Search by Author** (with author-to-books drill-down), inspect descriptions, and download EPUBs to `/Books/Audiobookshelf/` (configurable in Web UI).
- **Link Existing SD Books**: On the **Plugins** web page, use **Sync Existing SD Card Books** to scan EPUBs already on your SD card, match them to Audiobookshelf items, and write their `.meta.json` sidecars without re-downloading.
- **Offline Progress Outbox**: Closing a linked or downloaded book queues reading progress (`ebookProgress` and `isFinished`) and pushes it on your next Wi-Fi session.
- **Two-Way Sync in Reader**: Inside any Audiobookshelf book (with its `<book>.epub.meta.json` sidecar containing `abs_item_id`), open the reader menu and choose **Sync Progress** to compare and sync with Audiobookshelf (`ebookProgress` and `ebookLocation` EPUB CFI). If KOReader Sync is also configured, the device prompts you to choose between **Audiobookshelf** and **KOReader Sync**.
