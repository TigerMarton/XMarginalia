#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// Verify enums and layout logic as declared in CrossPointSettings.h
namespace CrossPointSettings {

enum STATUS_BAR_SLOT_ITEM : uint8_t {
  STATUS_BAR_SLOT_NONE = 0,
  STATUS_BAR_SLOT_BOOK_TITLE = 1,
  STATUS_BAR_SLOT_CHAPTER_TITLE = 2,
  STATUS_BAR_SLOT_CHAPTER_PAGE = 3,
  STATUS_BAR_SLOT_BOOK_PAGE = 4,
  STATUS_BAR_SLOT_BOOK_PERCENT = 5,
  STATUS_BAR_SLOT_PAGE_AND_PERCENT = 6,
  STATUS_BAR_SLOT_BATTERY = 7,
  STATUS_BAR_SLOT_CLOCK = 8,
  STATUS_BAR_SLOT_TIME_LEFT_CHAPTER = 9,
  STATUS_BAR_SLOT_TIME_LEFT_BOOK = 10,
  STATUS_BAR_SLOT_ITEM_COUNT
};

enum STATUS_BAR_PROGRESS_POSITION : uint8_t {
  STATUS_BAR_PROGRESS_POS_BOTTOM = 0,
  STATUS_BAR_PROGRESS_POS_TOP = 1,
  STATUS_BAR_PROGRESS_POS_COUNT
};

enum STATUS_BAR_PROGRESS_BAR : uint8_t { HIDE_PROGRESS = 0, BOOK_PROGRESS = 1, CHAPTER_PROGRESS = 2 };

struct StatusBarSpec {
  uint8_t topLeft = STATUS_BAR_SLOT_NONE;
  uint8_t topCenter = STATUS_BAR_SLOT_NONE;
  uint8_t topRight = STATUS_BAR_SLOT_NONE;
  uint8_t bottomLeft = STATUS_BAR_SLOT_BATTERY;
  uint8_t bottomMidLeft = STATUS_BAR_SLOT_NONE;
  uint8_t bottomCenter = STATUS_BAR_SLOT_CHAPTER_TITLE;
  uint8_t bottomMidRight = STATUS_BAR_SLOT_NONE;
  uint8_t bottomRight = STATUS_BAR_SLOT_PAGE_AND_PERCENT;

  uint8_t progressBarMode = HIDE_PROGRESS;
  uint8_t progressBarPosition = STATUS_BAR_PROGRESS_POS_BOTTOM;
  uint8_t progressBarHeightPx = 0;

  bool showBatteryPercent = false;
  bool clock12h = false;
  uint8_t xtcMode = 0;

  bool showChapterPageCount = false;
  bool showBookProgressPercent = false;
  uint8_t titleMode = 0;
  bool showBattery = false;
  uint8_t clockMode = 0;

  bool hasTopTextLane() const {
    return topLeft != STATUS_BAR_SLOT_NONE || topCenter != STATUS_BAR_SLOT_NONE || topRight != STATUS_BAR_SLOT_NONE;
  }
  bool hasBottomTextLane() const {
    return bottomLeft != STATUS_BAR_SLOT_NONE || bottomMidLeft != STATUS_BAR_SLOT_NONE ||
           bottomCenter != STATUS_BAR_SLOT_NONE || bottomMidRight != STATUS_BAR_SLOT_NONE ||
           bottomRight != STATUS_BAR_SLOT_NONE;
  }
  bool hasTopBar() const {
    return hasTopTextLane() || (showsProgressBar() && progressBarPosition == STATUS_BAR_PROGRESS_POS_TOP);
  }
  bool hasBottomBar() const {
    return hasBottomTextLane() || (showsProgressBar() && progressBarPosition == STATUS_BAR_PROGRESS_POS_BOTTOM);
  }
  bool showsProgressBar() const { return progressBarMode != HIDE_PROGRESS; }
  bool showsChapterTitle() const {
    return topLeft == STATUS_BAR_SLOT_CHAPTER_TITLE || topCenter == STATUS_BAR_SLOT_CHAPTER_TITLE ||
           topRight == STATUS_BAR_SLOT_CHAPTER_TITLE || bottomLeft == STATUS_BAR_SLOT_CHAPTER_TITLE ||
           bottomMidLeft == STATUS_BAR_SLOT_CHAPTER_TITLE || bottomCenter == STATUS_BAR_SLOT_CHAPTER_TITLE ||
           bottomMidRight == STATUS_BAR_SLOT_CHAPTER_TITLE || bottomRight == STATUS_BAR_SLOT_CHAPTER_TITLE ||
           titleMode == 1;
  }
  bool showsBookTitle() const {
    return topLeft == STATUS_BAR_SLOT_BOOK_TITLE || topCenter == STATUS_BAR_SLOT_BOOK_TITLE ||
           topRight == STATUS_BAR_SLOT_BOOK_TITLE || bottomLeft == STATUS_BAR_SLOT_BOOK_TITLE ||
           bottomMidLeft == STATUS_BAR_SLOT_BOOK_TITLE || bottomCenter == STATUS_BAR_SLOT_BOOK_TITLE ||
           bottomMidRight == STATUS_BAR_SLOT_BOOK_TITLE || bottomRight == STATUS_BAR_SLOT_BOOK_TITLE || titleMode == 2;
  }
  bool showsTitle() const { return showsChapterTitle() || showsBookTitle(); }
  bool showsClock() const {
    return topLeft == STATUS_BAR_SLOT_CLOCK || topCenter == STATUS_BAR_SLOT_CLOCK ||
           topRight == STATUS_BAR_SLOT_CLOCK || bottomLeft == STATUS_BAR_SLOT_CLOCK ||
           bottomMidLeft == STATUS_BAR_SLOT_CLOCK || bottomCenter == STATUS_BAR_SLOT_CLOCK ||
           bottomMidRight == STATUS_BAR_SLOT_CLOCK || bottomRight == STATUS_BAR_SLOT_CLOCK || clockMode != 0;
  }
  bool textLaneVisible(bool /*clockAvailable*/) const {
    return hasBottomTextLane() || hasTopTextLane() || showChapterPageCount || showBookProgressPercent || showBattery ||
           showsTitle();
  }
};

}  // namespace CrossPointSettings

static int checks = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    checks++;                                                          \
    if (!(cond)) {                                                     \
      fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      return 1;                                                        \
    }                                                                  \
  } while (0)

// Standalone formatSlotText for verification
void formatSlotText(char* buf, size_t bufSize, uint8_t slot, int currentPage, int pageCount, float bookProgress,
                    int currentBookPage, int bookPageCount, const char* bookTitle, const char* chapterTitle,
                    int timeToChapterMin, int timeToBookMin) {
  buf[0] = '\0';
  switch (slot) {
    case CrossPointSettings::STATUS_BAR_SLOT_BOOK_TITLE:
      if (bookTitle) snprintf(buf, bufSize, "%s", bookTitle);
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_TITLE:
      if (chapterTitle) snprintf(buf, bufSize, "%s", chapterTitle);
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_PAGE:
      snprintf(buf, bufSize, "%d/%d", currentPage, pageCount);
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_BOOK_PAGE:
      if (bookPageCount > 0) {
        snprintf(buf, bufSize, "%d/%d", currentBookPage, bookPageCount);
      } else {
        snprintf(buf, bufSize, "%d/%d", currentPage, pageCount);
      }
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_BOOK_PERCENT:
      snprintf(buf, bufSize, "%.0f%%", bookProgress);
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_PAGE_AND_PERCENT:
      snprintf(buf, bufSize, "%d/%d  %.0f%%", currentPage, pageCount, bookProgress);
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_TIME_LEFT_CHAPTER:
      if (timeToChapterMin >= 0) {
        if (timeToChapterMin >= 60) {
          snprintf(buf, bufSize, "%dh %dm left", timeToChapterMin / 60, timeToChapterMin % 60);
        } else {
          snprintf(buf, bufSize, "%dm left", timeToChapterMin);
        }
      }
      break;
    case CrossPointSettings::STATUS_BAR_SLOT_TIME_LEFT_BOOK:
      if (timeToBookMin >= 0) {
        if (timeToBookMin >= 60) {
          snprintf(buf, bufSize, "%dh %dm left", timeToBookMin / 60, timeToBookMin % 60);
        } else {
          snprintf(buf, bufSize, "%dm left", timeToBookMin);
        }
      }
      break;
    default:
      break;
  }
}

// Height calculation verification logic
int getTopStatusBarHeight(const CrossPointSettings::StatusBarSpec& sb, int verticalMargin, int progressMarginTop) {
  const bool hasText = sb.hasTopTextLane();
  const bool hasProgress =
      sb.showsProgressBar() && sb.progressBarPosition == CrossPointSettings::STATUS_BAR_PROGRESS_POS_TOP;
  if (!hasText && !hasProgress) return 0;
  return (hasText ? verticalMargin : 0) + (hasProgress ? (sb.progressBarHeightPx + progressMarginTop) : 0);
}

int getBottomStatusBarHeight(const CrossPointSettings::StatusBarSpec& sb, int verticalMargin, int progressMarginTop) {
  const bool hasText = sb.hasBottomTextLane();
  const bool hasProgress =
      sb.showsProgressBar() && sb.progressBarPosition == CrossPointSettings::STATUS_BAR_PROGRESS_POS_BOTTOM;
  if (!hasText && !hasProgress) return 0;
  return (hasText ? verticalMargin : 0) + (hasProgress ? (sb.progressBarHeightPx + progressMarginTop) : 0);
}

int main() {
  printf("Running modular status bar tests...\n");

  // 1. Default StatusBarSpec state
  CrossPointSettings::StatusBarSpec defSpec;
  CHECK(defSpec.topLeft == CrossPointSettings::STATUS_BAR_SLOT_NONE);
  CHECK(defSpec.topCenter == CrossPointSettings::STATUS_BAR_SLOT_NONE);
  CHECK(defSpec.topRight == CrossPointSettings::STATUS_BAR_SLOT_NONE);
  CHECK(defSpec.bottomLeft == CrossPointSettings::STATUS_BAR_SLOT_BATTERY);
  CHECK(defSpec.bottomCenter == CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_TITLE);
  CHECK(defSpec.bottomRight == CrossPointSettings::STATUS_BAR_SLOT_PAGE_AND_PERCENT);
  CHECK(defSpec.hasTopBar() == false);
  CHECK(defSpec.hasBottomBar() == true);
  CHECK(defSpec.hasTopTextLane() == false);
  CHECK(defSpec.hasBottomTextLane() == true);
  CHECK(defSpec.showsProgressBar() == false);

  // Height of default status bar
  constexpr int vMargin = 19;
  constexpr int pMargin = 1;
  CHECK(getTopStatusBarHeight(defSpec, vMargin, pMargin) == 0);
  CHECK(getBottomStatusBarHeight(defSpec, vMargin, pMargin) == vMargin);

  // 2. Top bar configuration
  CrossPointSettings::StatusBarSpec topSpec = defSpec;
  topSpec.topLeft = CrossPointSettings::STATUS_BAR_SLOT_CLOCK;
  CHECK(topSpec.hasTopBar() == true);
  CHECK(topSpec.hasTopTextLane() == true);
  CHECK(topSpec.showsClock() == true);
  CHECK(getTopStatusBarHeight(topSpec, vMargin, pMargin) == vMargin);

  // 3. Progress bar position & height
  CrossPointSettings::StatusBarSpec pbSpec = defSpec;
  pbSpec.progressBarMode = CrossPointSettings::STATUS_BAR_PROGRESS_BAR::BOOK_PROGRESS;
  pbSpec.progressBarPosition = CrossPointSettings::STATUS_BAR_PROGRESS_POS_TOP;
  pbSpec.progressBarHeightPx = 4;
  CHECK(pbSpec.showsProgressBar() == true);
  CHECK(pbSpec.hasTopBar() == true);
  CHECK(pbSpec.hasTopTextLane() == false);
  CHECK(getTopStatusBarHeight(pbSpec, vMargin, pMargin) == 4 + pMargin);

  pbSpec.progressBarPosition = CrossPointSettings::STATUS_BAR_PROGRESS_POS_BOTTOM;
  CHECK(pbSpec.hasTopBar() == false);
  CHECK(pbSpec.hasBottomBar() == true);
  CHECK(getBottomStatusBarHeight(pbSpec, vMargin, pMargin) == vMargin + 4 + pMargin);

  // 4. Slot formatting checks
  char buf[64];

  // Book title
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_BOOK_TITLE, 1, 10, 50.0f, 1, 100, "Moby Dick",
                 "Chapter 1", -1, -1);
  CHECK(strcmp(buf, "Moby Dick") == 0);

  // Chapter title
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_TITLE, 1, 10, 50.0f, 1, 100, "Moby Dick",
                 "Loomings", -1, -1);
  CHECK(strcmp(buf, "Loomings") == 0);

  // Chapter page
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_PAGE, 5, 25, 20.0f, 50, 250, "Moby Dick",
                 "Loomings", -1, -1);
  CHECK(strcmp(buf, "5/25") == 0);

  // Book page
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_BOOK_PAGE, 5, 25, 20.0f, 50, 250, "Moby Dick",
                 "Loomings", -1, -1);
  CHECK(strcmp(buf, "50/250") == 0);

  // Book percent
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_BOOK_PERCENT, 5, 25, 20.4f, 50, 250, "Moby Dick",
                 "Loomings", -1, -1);
  CHECK(strcmp(buf, "20%") == 0);

  // Page and percent
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_PAGE_AND_PERCENT, 5, 25, 20.4f, 50, 250,
                 "Moby Dick", "Loomings", -1, -1);
  CHECK(strcmp(buf, "5/25  20%") == 0);

  // Time left in chapter (< 60m)
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_TIME_LEFT_CHAPTER, 5, 25, 20.0f, 50, 250,
                 "Moby Dick", "Loomings", 18, 140);
  CHECK(strcmp(buf, "18m left") == 0);

  // Time left in book (>= 60m)
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_TIME_LEFT_BOOK, 5, 25, 20.0f, 50, 250,
                 "Moby Dick", "Loomings", 18, 140);
  CHECK(strcmp(buf, "2h 20m left") == 0);

  // None slot produces empty
  formatSlotText(buf, sizeof(buf), CrossPointSettings::STATUS_BAR_SLOT_NONE, 5, 25, 20.0f, 50, 250, "Moby Dick",
                 "Loomings", 18, 140);
  CHECK(strcmp(buf, "") == 0);

  // 5. Title & Clock resolution
  CHECK(defSpec.showsTitle() == true);  // center is chapter title
  CHECK(defSpec.showsChapterTitle() == true);
  CHECK(defSpec.showsBookTitle() == false);
  defSpec.topLeft = CrossPointSettings::STATUS_BAR_SLOT_BOOK_TITLE;
  CHECK(defSpec.showsBookTitle() == true);
  CHECK(defSpec.showsClock() == false);
  defSpec.topRight = CrossPointSettings::STATUS_BAR_SLOT_CLOCK;
  CHECK(defSpec.showsClock() == true);

  // 6. Bottom mid-left and mid-right slots activate lane and resolve flags
  CrossPointSettings::StatusBarSpec midSpec;
  midSpec.bottomLeft = CrossPointSettings::STATUS_BAR_SLOT_NONE;
  midSpec.bottomCenter = CrossPointSettings::STATUS_BAR_SLOT_NONE;
  midSpec.bottomRight = CrossPointSettings::STATUS_BAR_SLOT_NONE;
  CHECK(midSpec.hasBottomTextLane() == false);
  midSpec.bottomMidLeft = CrossPointSettings::STATUS_BAR_SLOT_CLOCK;
  CHECK(midSpec.hasBottomTextLane() == true);
  CHECK(midSpec.showsClock() == true);
  midSpec.bottomMidLeft = CrossPointSettings::STATUS_BAR_SLOT_NONE;
  midSpec.bottomMidRight = CrossPointSettings::STATUS_BAR_SLOT_BOOK_TITLE;
  CHECK(midSpec.hasBottomTextLane() == true);
  CHECK(midSpec.showsBookTitle() == true);

  printf("%d checks, 0 failed\n", checks);
  return 0;
}
