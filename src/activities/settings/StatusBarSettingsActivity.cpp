#include "StatusBarSettingsActivity.h"

#include <GfxRenderer.h>
#include <HalClock.h>
#include <I18n.h>

#include <cstring>
#include <memory>

#include "CrossPointSettings.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
enum MenuItem {
  ITEM_TOP_LEFT = 0,
  ITEM_TOP_CENTER,
  ITEM_TOP_RIGHT,
  ITEM_BOTTOM_LEFT,
  ITEM_BOTTOM_MID_LEFT,
  ITEM_BOTTOM_CENTER,
  ITEM_BOTTOM_MID_RIGHT,
  ITEM_BOTTOM_RIGHT,
  ITEM_PROGRESS_BAR,
  ITEM_PROGRESS_BAR_POSITION,
  ITEM_PROGRESS_BAR_THICKNESS,
  ITEM_BATTERY_PERCENT,
  ITEM_RESET_DEFAULT,
  ITEM_COUNT
};

static_assert(ITEM_COUNT == StatusBarSettingsActivity::MAX_STATUS_BAR_ITEMS,
              "keep StatusBarSettingsActivity::MAX_STATUS_BAR_ITEMS in sync with ITEM_COUNT");

const StrId menuNames[ITEM_COUNT] = {
    StrId::STR_TOP_BAR_LEFT,          StrId::STR_TOP_BAR_CENTER,         StrId::STR_TOP_BAR_RIGHT,
    StrId::STR_BOTTOM_BAR_LEFT,       StrId::STR_BOTTOM_BAR_MID_LEFT,    StrId::STR_BOTTOM_BAR_CENTER,
    StrId::STR_BOTTOM_BAR_MID_RIGHT,  StrId::STR_BOTTOM_BAR_RIGHT,       StrId::STR_PROGRESS_BAR,
    StrId::STR_PROGRESS_BAR_POSITION, StrId::STR_PROGRESS_BAR_THICKNESS, StrId::STR_HIDE_BATTERY,
    StrId::STR_RESET_DEFAULT,
};

constexpr int SLOT_ITEM_CHOICES = CrossPointSettings::STATUS_BAR_SLOT_ITEM_COUNT;
const StrId slotItemNames[SLOT_ITEM_CHOICES] = {
    StrId::STR_SLOT_NONE,
    StrId::STR_SLOT_BOOK_TITLE,
    StrId::STR_SLOT_CHAPTER_TITLE,
    StrId::STR_SLOT_CHAPTER_PAGE,
    StrId::STR_SLOT_BOOK_PAGE,
    StrId::STR_SLOT_BOOK_PERCENT,
    StrId::STR_SLOT_PAGE_AND_PERCENT,
    StrId::STR_SLOT_BATTERY,
    StrId::STR_SLOT_CLOCK,
    StrId::STR_SLOT_TIME_LEFT_CHAPTER,
    StrId::STR_SLOT_TIME_LEFT_BOOK,
};

constexpr int PROGRESS_BAR_ITEMS = 3;
const StrId progressBarNames[PROGRESS_BAR_ITEMS] = {StrId::STR_BOOK, StrId::STR_CHAPTER, StrId::STR_HIDE};

constexpr int POSITION_ITEMS = 2;
const StrId positionNames[POSITION_ITEMS] = {StrId::STR_BOTTOM, StrId::STR_TOP};

constexpr int PROGRESS_BAR_THICKNESS_ITEMS = 3;
const StrId progressBarThicknessNames[PROGRESS_BAR_THICKNESS_ITEMS] = {
    StrId::STR_PROGRESS_BAR_THIN, StrId::STR_PROGRESS_BAR_MEDIUM, StrId::STR_PROGRESS_BAR_THICK};

const int verticalPreviewTextPadding = 40;
}  // namespace

StatusBarSettingsActivity::StatusBarSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("StatusBarSettings", renderer, mappedInput) {}

void StatusBarSettingsActivity::onEnter() {
  UiListActivity::onEnter();

  visibleItemCount = ITEM_COUNT;

  if (SETTINGS.statusBarProgressBar >= PROGRESS_BAR_ITEMS) {
    SETTINGS.statusBarProgressBar = CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS;
  }

  if (SETTINGS.statusBarProgressBarThickness >= PROGRESS_BAR_THICKNESS_ITEMS) {
    SETTINGS.statusBarProgressBarThickness = CrossPointSettings::STATUS_BAR_PROGRESS_BAR_THICKNESS::PROGRESS_BAR_NORMAL;
  }

  if (SETTINGS.statusBarProgressBarPosition >= POSITION_ITEMS) {
    SETTINGS.statusBarProgressBarPosition = CrossPointSettings::STATUS_BAR_PROGRESS_POS_BOTTOM;
  }

  for (int i = 0; i < visibleItemCount; i++) {
    rowItems_[i].label = I18N.get(menuNames[i]);
    rowItems_[i].actionValue = static_cast<int16_t>(i);
  }
}

bool StatusBarSettingsActivity::handleCustomInput() {
  return optionPopup.handleInput(mappedInput, [this] { requestUpdate(); });
}

void StatusBarSettingsActivity::activateIndex(const int index) {
  if (optionPopup.isActive()) return;
  nav.selected = index;
  app.clearTapFlash();
  handleSelection();
  requestUpdate();
}

void StatusBarSettingsActivity::handleSelection() {
  auto slotCallback = [this](uint8_t& targetSlot) {
    optionPopup.show(StrId::STR_CUSTOMISE_STATUS_BAR, slotItemNames, SLOT_ITEM_CHOICES, targetSlot,
                     [this, &targetSlot](int idx) {
                       targetSlot = static_cast<uint8_t>(idx);
                       SETTINGS.saveToFile();
                     });
  };

  switch (nav.selected) {
    case ITEM_TOP_LEFT:
      slotCallback(SETTINGS.statusBarTopLeft);
      return;
    case ITEM_TOP_CENTER:
      slotCallback(SETTINGS.statusBarTopCenter);
      return;
    case ITEM_TOP_RIGHT:
      slotCallback(SETTINGS.statusBarTopRight);
      return;
    case ITEM_BOTTOM_LEFT:
      slotCallback(SETTINGS.statusBarBottomLeft);
      return;
    case ITEM_BOTTOM_MID_LEFT:
      slotCallback(SETTINGS.statusBarBottomMidLeft);
      return;
    case ITEM_BOTTOM_CENTER:
      slotCallback(SETTINGS.statusBarBottomCenter);
      return;
    case ITEM_BOTTOM_MID_RIGHT:
      slotCallback(SETTINGS.statusBarBottomMidRight);
      return;
    case ITEM_BOTTOM_RIGHT:
      slotCallback(SETTINGS.statusBarBottomRight);
      return;
    case ITEM_PROGRESS_BAR:
      optionPopup.show(StrId::STR_PROGRESS_BAR, progressBarNames, PROGRESS_BAR_ITEMS, SETTINGS.statusBarProgressBar,
                       [this](int idx) {
                         SETTINGS.statusBarProgressBar = idx;
                         SETTINGS.saveToFile();
                       });
      return;
    case ITEM_PROGRESS_BAR_POSITION:
      optionPopup.show(StrId::STR_PROGRESS_BAR_POSITION, positionNames, POSITION_ITEMS,
                       SETTINGS.statusBarProgressBarPosition, [this](int idx) {
                         SETTINGS.statusBarProgressBarPosition = idx;
                         SETTINGS.saveToFile();
                       });
      return;
    case ITEM_PROGRESS_BAR_THICKNESS:
      optionPopup.show(StrId::STR_PROGRESS_BAR_THICKNESS, progressBarThicknessNames, PROGRESS_BAR_THICKNESS_ITEMS,
                       SETTINGS.statusBarProgressBarThickness, [this](int idx) {
                         SETTINGS.statusBarProgressBarThickness = idx;
                         SETTINGS.saveToFile();
                       });
      return;
    case ITEM_BATTERY_PERCENT:
      SETTINGS.hideBatteryPercentage = (SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_NEVER)
                                           ? CrossPointSettings::HIDE_ALWAYS
                                           : CrossPointSettings::HIDE_NEVER;
      break;
    case ITEM_RESET_DEFAULT:
      SETTINGS.statusBarTopLeft = CrossPointSettings::STATUS_BAR_SLOT_NONE;
      SETTINGS.statusBarTopCenter = CrossPointSettings::STATUS_BAR_SLOT_NONE;
      SETTINGS.statusBarTopRight = CrossPointSettings::STATUS_BAR_SLOT_NONE;
      SETTINGS.statusBarBottomLeft = CrossPointSettings::STATUS_BAR_SLOT_BATTERY;
      SETTINGS.statusBarBottomMidLeft = CrossPointSettings::STATUS_BAR_SLOT_NONE;
      SETTINGS.statusBarBottomCenter = CrossPointSettings::STATUS_BAR_SLOT_CHAPTER_TITLE;
      SETTINGS.statusBarBottomMidRight = CrossPointSettings::STATUS_BAR_SLOT_NONE;
      SETTINGS.statusBarBottomRight = CrossPointSettings::STATUS_BAR_SLOT_PAGE_AND_PERCENT;
      SETTINGS.statusBarProgressBar = CrossPointSettings::STATUS_BAR_PROGRESS_BAR::HIDE_PROGRESS;
      SETTINGS.statusBarProgressBarPosition = CrossPointSettings::STATUS_BAR_PROGRESS_POS_BOTTOM;
      SETTINGS.statusBarProgressBarThickness =
          CrossPointSettings::STATUS_BAR_PROGRESS_BAR_THICKNESS::PROGRESS_BAR_NORMAL;
      break;
    default:
      return;
  }
  SETTINGS.saveToFile();
}

std::string StatusBarSettingsActivity::rowValueText(const int index) {
  auto slotName = [](uint8_t slot) -> std::string {
    if (slot < SLOT_ITEM_CHOICES) {
      return I18N.get(slotItemNames[slot]);
    }
    return "";
  };

  switch (index) {
    case ITEM_TOP_LEFT:
      return slotName(SETTINGS.statusBarTopLeft);
    case ITEM_TOP_CENTER:
      return slotName(SETTINGS.statusBarTopCenter);
    case ITEM_TOP_RIGHT:
      return slotName(SETTINGS.statusBarTopRight);
    case ITEM_BOTTOM_LEFT:
      return slotName(SETTINGS.statusBarBottomLeft);
    case ITEM_BOTTOM_MID_LEFT:
      return slotName(SETTINGS.statusBarBottomMidLeft);
    case ITEM_BOTTOM_CENTER:
      return slotName(SETTINGS.statusBarBottomCenter);
    case ITEM_BOTTOM_MID_RIGHT:
      return slotName(SETTINGS.statusBarBottomMidRight);
    case ITEM_BOTTOM_RIGHT:
      return slotName(SETTINGS.statusBarBottomRight);
    case ITEM_PROGRESS_BAR:
      return I18N.get(progressBarNames[SETTINGS.statusBarProgressBar]);
    case ITEM_PROGRESS_BAR_POSITION:
      return I18N.get(positionNames[SETTINGS.statusBarProgressBarPosition]);
    case ITEM_PROGRESS_BAR_THICKNESS:
      return I18N.get(progressBarThicknessNames[SETTINGS.statusBarProgressBarThickness]);
    default:
      return "";
  }
}

void StatusBarSettingsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int statusBarHeight = UITheme::getInstance().getBottomStatusBarHeight();
  const int topBarHeight = UITheme::getInstance().getTopStatusBarHeight();
  const int totalPreviewHeight = statusBarHeight + (topBarHeight > 0 ? (topBarHeight + 6) : 0);
  const auto previewFooter =
      static_cast<int16_t>(totalPreviewHeight + verticalPreviewTextPadding + metrics.verticalSpacing);
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight + previewFooter), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  for (int i = 0; i < visibleItemCount; i++) {
    rowValues_[i] = rowValueText(i);
    rowItems_[i].value = rowValues_[i].empty() ? nullptr : rowValues_[i].c_str();
  }
  GUI.setCheckboxRow(rowItems_[ITEM_BATTERY_PERCENT],
                     SETTINGS.hideBatteryPercentage == CrossPointSettings::HIDE_ALWAYS);

  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(visibleItemCount);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.valueInset = 8;
  props.labelText = screen.theme().smallText;
  props.labelText.maxLines = 2;
  syncListViewport(screen, props);
  screen.list(props);
}

void StatusBarSettingsActivity::render(RenderLock&&) {
  if (optionPopup.processRender(renderer, mappedInput)) return;

  renderer.clearScreen();

  auto metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_CUSTOMISE_STATUS_BAR));

  renderUi();

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  const int statusBarHeight = UITheme::getInstance().getBottomStatusBarHeight();
  const int topBarHeight = UITheme::getInstance().getTopStatusBarHeight();
  const int totalPreviewHeight = statusBarHeight + (topBarHeight > 0 ? (topBarHeight + 6) : 0);

  // Live modular status bar preview directly above the button hints
  BaseTheme::StatusBarContext ctx;
  ctx.bookProgress = 75.0f;
  ctx.currentPage = 8;
  ctx.pageCount = 32;
  std::string exBook = tr(STR_EXAMPLE_BOOK);
  std::string exChapter = tr(STR_EXAMPLE_CHAPTER);
  ctx.bookTitle = exBook.c_str();
  ctx.chapterTitle = exChapter.c_str();
  ctx.timeToChapterMin = 14;
  ctx.timeToBookMin = 135;
  ctx.paddingBottom = metrics.buttonHintsHeight;
  ctx.paddingTop = renderer.getScreenHeight() - metrics.buttonHintsHeight - totalPreviewHeight;
  ctx.fillMargin = false;

  GUI.drawModularStatusBar(renderer, ctx);

  renderer.drawCenteredText(
      UI_10_FONT_ID,
      renderer.getScreenHeight() - metrics.buttonHintsHeight - totalPreviewHeight - verticalPreviewTextPadding,
      tr(STR_PREVIEW));

  renderer.displayBuffer();
}
