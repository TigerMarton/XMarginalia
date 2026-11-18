# BOOX Picco

The BOOX Picco runs XMarginalia with one catch that applies to every board write: **its partition table is the maker's, not CrossPoint's**, and overwriting that table drops the vendor's `safeboot` recovery partition. Every build, flasher, and test rule here writes only the app slot at `0xE0000` and refuses anything else.

## References

- **CrossPoint / CrossPlay upstream Picco work**: `picco` env and `partitions_picco.csv` (#3947), panel variants (#4146), slide switch hardware setting (#4234), app-only upload (#4248, fixed in #4510), `scripts_local/app_slot_guard.py`, and `#4362` EPD display/image enhancements.
- **FreeInk SDK `PICCO` profile** (`BoardConfig.h`): pins, panel, TMA525C touch, warm/cool frontlight, charger, and insets.
- **CrossPoint simulator Picco profile**: `simulator_picco` in `platformio.sim.ini` with `-DFREEINK_DEVICE_PICCO=1`.

## The partitions are the vendor's

`partitions_picco.csv` keeps the stock retail layout:

| Partition  | Offset     | Size       | Purpose                                              |
| ---------- | ---------- | ---------- | ---------------------------------------------------- |
| `nvs`      | `0x9000`   | `0xC000`   | Vendor NVS                                           |
| `otadata`  | `0x15000`  | `0x2000`   |                                                      |
| `safeboot` | `0x20000`  | `0xC0000`  | Factory app: vendor recovery image                   |
| `app`      | `0xE0000`  | `0xE70000` | `ota_0`: the single slot XMarginalia is written into |
| `spiffs`   | `0xF50000` | `0x80000`  |                                                      |
| `coredump` | `0xFD0000` | `0x10000`  |                                                      |

Every path enforces `0xE0000`-only writes and disables self-OTA (`UpdateSlot.h` prevents `esp_ota_get_next_update_partition()` from returning the running slot):

| Path | Behaviour on BOOX Picco |
| --- | --- |
| `./scripts_local/build-release.sh picco` | Builds `gh_release_picco`; `app_slot_guard.py` removes `firmware.factory.bin` post-build |
| `./scripts_local/flash-device.sh picco` | Auto-detects `safeboot` in `0x8000` or targets `picco`, flashing `.pio/build/gh_release_picco/firmware.bin` strictly at `0xE0000` |
| `pio run -e gh_release_picco -t upload` | `scripts/app_only_upload.py` strips bootloader/partition/boot_app0 image pairs; `scripts_local/app_slot_guard.py` refuses erase/fs/debug targets and wrong offsets |
| `site/assets/install.js` | Reads device partition table first, stops cleanly on `safeboot` without erasing |
| `scripts_local/usb-flash.sh` / `wifi-flash.sh` | Refuses `picco` env and `safeboot` tables; device returns "no OTA partition" |
| Settings -> Check for Updates / SD Update | Hidden on Picco in `SettingsActivity.cpp`; boot recovery firmware shortcut disabled in `main.cpp` |

## Hardware & What Apps See

- **Screen**: 480x800 portrait (`uiScale` 1.2), TMA525C capacitive touch, no capacitive Home key (Home is bottom-edge up-swipe; Back is left-edge swipe).
- **Physical Keys**: Up (GPIO21), Down (GPIO9), Power (GPIO2). Pressing **Up + Down** outside the reader restores reader touch controls (`SETTINGS.touchReaderControls = TOUCH_READER_ON`) in case touch was toggled off accidentally.
- **Slide Switch** (GPIO6): Bound via **Settings -> Controls -> Slide Switch** (Frontlight toggle default, Night Mode, or Reader Touch Controls).
- **Simulator**: Run `CROSSPOINT_SIM_ENV=simulator_picco ./scripts_local/sim-shot.sh ...` (or `pio run -e simulator_picco`) to test the 480x800 portrait layout (`T` toggles the slide switch).

## Memory & TLS Architecture

- **Display buffers in PSRAM**: `FREEINK_FB_PSRAM=1` + `freeink-sdk`'s `_asyncShadow` PSRAM allocation keep internal DRAM available.
- **All-PSRAM wolfSSL heap in `freeink-sdk`**: Rather than a Picco-only hook, XMarginalia routes wolfSSL allocations (`XMALLOC`/`XFREE`/`XREALLOC`) to SPIRAM for all `BOARD_HAS_PSRAM` boards inside `freeink-sdk`'s `SecureClient.cpp`, keeping TLS handshakes reliable across OPDS, Audiobookshelf, AI Companion, Hacker News, xkcd, and Study sync.

Before your first flash, back up the full 16 MB stock flash once:

```bash
esptool.py --chip esp32s3 read_flash 0 0x1000000 picco-stock.bin
```
