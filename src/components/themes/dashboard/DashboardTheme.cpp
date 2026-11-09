#include "DashboardTheme.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include "RecentBooksStore.h"
#include "activities/reader/GlobalReadingStats.h"
#include "fontIds.h"

void DashboardTheme::drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                                    const std::function<std::string(int index)>& buttonLabel,
                                    const std::function<UIIcon(int index)>& /*rowIcon*/, int rowSpacing,
                                    int rowHeight) const {
  const int height = rowHeight > 0 ? rowHeight : DashboardMetrics::values.menuRowHeight;
  const int spacing = rowSpacing >= 0 ? rowSpacing : DashboardMetrics::values.menuSpacing;
  const int sidePadding = DashboardMetrics::values.contentSidePadding;

  for (int i = 0; i < buttonCount; i++) {
    const int tileY = rect.y + i * (height + spacing);
    const int tileX = rect.x + sidePadding;
    const int tileW = rect.width - sidePadding * 2;

    if (selectedIndex == i) {
      renderer.fillRoundedRect(tileX, tileY, tileW, height, 6, Color::Black);
    } else {
      renderer.drawRoundedRect(tileX, tileY, tileW, height, 1, 6, true);
    }

    const std::string labelStr = buttonLabel(i);
    const char* label = labelStr.c_str();
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, label);
    const int textX = tileX + (tileW - textWidth) / 2;
    const int lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
    const int textY = tileY + (height - lineHeight) / 2;

    renderer.drawText(UI_10_FONT_ID, textX, textY, label, selectedIndex != i);
  }
}

void DashboardTheme::drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                                        const int /*selectorIndex*/, bool& /*coverRendered*/,
                                        bool& /*coverBufferStored*/, bool& /*bufferRestored*/,
                                        std::function<bool()> /*storeCoverBuffer*/) const {
  const int sidePadding = DashboardMetrics::values.contentSidePadding;
  const int cardX = rect.x + sidePadding;
  const int cardW = rect.width - sidePadding * 2;
  const int cardY = rect.y;
  const int cardH = rect.height;

  renderer.drawRoundedRect(cardX, cardY, cardW, cardH, 1, 6, true);

  // Top stats pill banner inside card
  auto stats = makeUniqueNoThrow<GlobalReadingStats>(GlobalReadingStats::load());
  const uint16_t streak = stats ? stats->displayLongestReadingStreak() : 0;
  const uint32_t hours = stats ? (stats->totalReadingSeconds / 3600) : 0;
  const uint16_t completed = stats ? stats->completedBooks : 0;

  char statsBuf[64];
  snprintf(statsBuf, sizeof(statsBuf), "%u %s %s  |  %u %s  |  %u %s", streak,
           streak == 1 ? tr(STR_STATS_DAY) : tr(STR_STATS_DAYS), tr(STR_STATS_STREAK_SHORT),
           static_cast<unsigned>(hours), tr(STR_STATS_HOURS_SHORT), static_cast<unsigned>(completed),
           tr(STR_STATS_COMPLETED_LBL));

  const int statsW = renderer.getTextWidth(SMALL_FONT_ID, statsBuf);
  const int statsX = cardX + (cardW - statsW) / 2;
  const int statsY = cardY + 12;

  renderer.fillRoundedRect(cardX + 8, cardY + 8, cardW - 16, 26, 4, Color::LightGray);
  renderer.drawText(SMALL_FONT_ID, statsX, statsY, statsBuf, true, EpdFontFamily::REGULAR);

  if (!recentBooks.empty()) {
    const auto& book = recentBooks.front();
    const int contentY = cardY + 44;

    // Truncate title and author to fit card
    const int maxTextW = cardW - 32;
    std::string titleTrunc = renderer.truncatedText(UI_10_FONT_ID, book.title.c_str(), maxTextW, EpdFontFamily::BOLD);
    renderer.drawText(UI_10_FONT_ID, cardX + 16, contentY, titleTrunc.c_str(), true, EpdFontFamily::BOLD);

    if (!book.author.empty()) {
      std::string authorTrunc = renderer.truncatedText(SMALL_FONT_ID, book.author.c_str(), maxTextW);
      renderer.drawText(SMALL_FONT_ID, cardX + 16, contentY + 28, authorTrunc.c_str());
    }

    const int resumeY = cardY + cardH - 24;
    renderer.drawText(SMALL_FONT_ID, cardX + 16, resumeY, tr(STR_RESUME), true, EpdFontFamily::BOLD);
  } else {
    const char* hint = tr(STR_NO_OPEN_BOOK);
    const int hintW = renderer.getTextWidth(SMALL_FONT_ID, hint);
    renderer.drawText(SMALL_FONT_ID, cardX + (cardW - hintW) / 2, cardY + cardH / 2 + 6, hint);
  }
}
