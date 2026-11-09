#include "BootActivity.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  HalFile bootFile;
  if (Storage.openFileForRead("BOOT", "/bootscreen.bmp", bootFile) ||
      Storage.openFileForRead("BOOT", "/boot.bmp", bootFile)) {
    Bitmap bitmap(bootFile);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      renderer.clearScreen();
      const int bmpW = bitmap.getWidth();
      const int bmpH = bitmap.getHeight();
      const int x = (bmpW < pageWidth) ? (pageWidth - bmpW) / 2 : 0;
      const int y = (bmpH < pageHeight) ? (pageHeight - bmpH) / 2 : 0;
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0, false);
      renderer.displayBuffer();
      return;
    }
  }

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2 + 70, tr(STR_CROSSPLAY), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight / 2 + 95, tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, pageHeight - 30, CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
