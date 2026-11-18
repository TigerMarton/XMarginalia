#!/bin/bash
# Package SD-card plugins under plugins/ into dist/*.zip release artifacts.
set -euo pipefail

SCRIPTS_DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
REPO="$(cd "$SCRIPTS_DIR/.." && pwd)"
cd "$REPO"

mkdir -p dist

python3 - <<'PY'
import os
import zipfile

plugins_dir = "plugins"
out_zip = os.path.join("dist", "xmarginalia-audiobookshelf-plugin.zip")
plugin_name = "audiobookshelf"
src_dir = os.path.join(plugins_dir, plugin_name)

files = []
for root, _, filenames in os.walk(src_dir):
    for fn in sorted(filenames):
        full = os.path.join(root, fn)
        rel = os.path.relpath(full, plugins_dir)
        files.append((full, rel))

files.sort(key=lambda pair: pair[1])
with zipfile.ZipFile(out_zip, "w", compression=zipfile.ZIP_DEFLATED) as zf:
    for full, rel in files:
        zi = zipfile.ZipInfo(rel, date_time=(2026, 1, 1, 0, 0, 0))
        zi.compress_type = zipfile.ZIP_DEFLATED
        with open(full, "rb") as fh:
            zf.writestr(zi, fh.read())

print(f"Packaged {out_zip} ({os.path.getsize(out_zip)} bytes):")
for _, rel in files:
    print(f"  - {rel}")
PY
