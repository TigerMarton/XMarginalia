#include "TabletopScreens.h"

#include <FreeInkUIIcon.h>

#include <cstdio>
#include <cstring>

#include "../link/LinkScreens.h"
#include "../ui/ToyboxFormat.h"
#include "../ui/ToyboxIcons.h"

namespace tabletopui {

namespace {

void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kUiFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

}  // namespace

void buildLobby(toybox::Screen& screen, const LobbyModel& model) {
  const int pageCount = kPageCount;
  int page = model.page;
  if (page < 0) page = 0;
  if (page >= pageCount) page = pageCount - 1;

  char counter[32];
  std::snprintf(counter, sizeof(counter), "%d/%d", page + 1, pageCount);
  toyboxChrome(screen, "TABLETOP", (pageCount > 1) ? counter : "SUITE");

  const fui::Rect line = screen.takeTop(28);
  fui::TextStyle small;
  small.font = toybox::kTileFont;
  small.align = fui::TextAlign::Left;
  screen.target().text(line, "CLASSIC BOARD GAMES & SANDBOX", small);
  screen.target().fill(fui::makeRect(line.x, static_cast<int16_t>(line.bottom() + 6), line.width, toybox::kRule),
                       fui::Paint::solid(fui::Color::Black));

  fui::ListItem allRows[static_cast<int>(tabletop::LobbyRow::Count)] = {};
  allRows[static_cast<int>(tabletop::LobbyRow::Reversi)].label = "REVERSI";
  allRows[static_cast<int>(tabletop::LobbyRow::Reversi)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Reversi);

  allRows[static_cast<int>(tabletop::LobbyRow::Mancala)].label = "MANCALA";
  allRows[static_cast<int>(tabletop::LobbyRow::Mancala)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Mancala);

  allRows[static_cast<int>(tabletop::LobbyRow::Morris)].label = "NINE MEN'S MORRIS";
  allRows[static_cast<int>(tabletop::LobbyRow::Morris)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Morris);

  allRows[static_cast<int>(tabletop::LobbyRow::Santorini)].label = "SANTORINI";
  allRows[static_cast<int>(tabletop::LobbyRow::Santorini)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Santorini);

  allRows[static_cast<int>(tabletop::LobbyRow::Quoridor)].label = "QUORIDOR";
  allRows[static_cast<int>(tabletop::LobbyRow::Quoridor)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Quoridor);

  allRows[static_cast<int>(tabletop::LobbyRow::Onitama)].label = "ONITAMA";
  allRows[static_cast<int>(tabletop::LobbyRow::Onitama)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Onitama);

  allRows[static_cast<int>(tabletop::LobbyRow::Dobutsu)].label = "DOBUTSU SHOGI";
  allRows[static_cast<int>(tabletop::LobbyRow::Dobutsu)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Dobutsu);

  allRows[static_cast<int>(tabletop::LobbyRow::Dominoes)].label = "DOMINOES";
  allRows[static_cast<int>(tabletop::LobbyRow::Dominoes)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Dominoes);

  allRows[static_cast<int>(tabletop::LobbyRow::LiarsDice)].label = "LIAR'S DICE";
  allRows[static_cast<int>(tabletop::LobbyRow::LiarsDice)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::LiarsDice);

  allRows[static_cast<int>(tabletop::LobbyRow::Backgammon)].label = "BACKGAMMON";
  allRows[static_cast<int>(tabletop::LobbyRow::Backgammon)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Backgammon);

  allRows[static_cast<int>(tabletop::LobbyRow::Sandbox)].label = "TABLETOP SANDBOX";
  allRows[static_cast<int>(tabletop::LobbyRow::Sandbox)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::Sandbox);

  allRows[static_cast<int>(tabletop::LobbyRow::About)].label = "ABOUT TABLETOP";
  allRows[static_cast<int>(tabletop::LobbyRow::About)].actionValue = static_cast<int16_t>(tabletop::LobbyRow::About);

  // Take bottom page bar if multiple pages exist
  if (pageCount > 1) {
    constexpr int16_t kPageBarHeight = 44;
    const fui::Rect bar = screen.takeBottom(kPageBarHeight, toybox::kGutter);

    constexpr int16_t kPitch = 44;
    const int16_t pitch =
        kPitch * pageCount <= bar.width ? kPitch : static_cast<int16_t>(bar.width / pageCount);
    const int16_t clusterX = static_cast<int16_t>(bar.x + (bar.width - pitch * pageCount) / 2);

    fui::StyleSet invisible;
    invisible.explicitlySet = true;

    const int16_t inset = 2;
    const int16_t cellH = static_cast<int16_t>(bar.height - 2 * inset);
    for (int p = 0; p < pageCount; ++p) {
      const fui::Rect target =
          fui::makeRect(static_cast<int16_t>(clusterX + p * pitch), bar.y, pitch, static_cast<int16_t>(bar.height));
      fui::ButtonProps jump;
      jump.action = ActionGoToPage;
      jump.value = static_cast<int16_t>(p);
      jump.styles = invisible;
      jump.minTouchSize = 0;
      screen.button(jump, target);

      const fui::Rect cell =
          fui::makeRect(static_cast<int16_t>(target.x + inset), static_cast<int16_t>(target.y + inset),
                        static_cast<int16_t>(pitch - 2 * inset), cellH);
      const bool here = (p == page);
      if (here) {
        screen.target().fill(cell, fui::Paint::solid(fui::Color::Black), 10);
      } else {
        screen.target().stroke(cell, fui::Paint::solid(fui::Color::Black), 1, 10);
      }

      char number[toybox::kIntTextChars];
      snprintf(number, sizeof(number), "%d", p + 1);
      fui::TextStyle style;
      style.font = toybox::kUiFont;
      style.align = fui::TextAlign::Center;
      style.color = here ? fui::Color::White : fui::Color::Black;
      screen.target().text(toybox::inkCentred(cell, toybox::kUiCut), number, style);
    }
  }

  // Slice rows for the current page
  const int startIdx = page * kRowsPerPage;
  int onThisPage = static_cast<int>(tabletop::LobbyRow::Count) - startIdx;
  if (onThisPage > kRowsPerPage) onThisPage = kRowsPerPage;
  if (onThisPage < 0) onThisPage = 0;

  fui::ListItem pageRows[kRowsPerPage];
  for (int i = 0; i < onThisPage; ++i) {
    pageRows[i] = allRows[startIdx + i];
  }

  fui::ListProps list;
  list.items = pageRows;
  list.count = static_cast<uint16_t>(onThisPage);
  list.topIndex = 0;
  if (model.selected >= startIdx && model.selected < startIdx + onThisPage) {
    list.selectedIndex = static_cast<int16_t>(model.selected - startIdx);
  } else {
    list.selectedIndex = -1;
  }
  list.action = ActionLobbyRow;
  list.rowHeight = toybox::kRowHeight;
  list.rowGap = screen.theme().listRowGap;

  screen.list(list);

  const fui::Rect content = screen.contentRect();
  for (int i = 0; i < onThisPage; ++i) {
    if (pageRows[i].actionValue != static_cast<int16_t>(tabletop::LobbyRow::About) &&
        pageRows[i].actionValue != static_cast<int16_t>(tabletop::LobbyRow::Sandbox)) {
      toybox::iconAtRowRight(screen, content, i, 0, linkui::nearbyMark(), list.selectedIndex == i);
    }
  }
}

void buildAbout(toybox::Screen& screen) {
  toyboxChrome(screen, "ABOUT TABLETOP");

  fui::ButtonProps btn;
  btn.label = "BACK TO LOBBY";
  btn.action = ActionBackToLobby;
  screen.button(btn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect content = screen.contentRect();

  fui::TextStyle hStyle;
  hStyle.font = toybox::kUiFont;
  hStyle.align = fui::TextAlign::Center;
  hStyle.color = fui::Color::Black;

  const fui::Rect titleRect = fui::makeRect(content.x, static_cast<int16_t>(content.y + 12), content.width, 32);
  screen.target().text(titleRect, "CROSSPLAY TABLETOP SUITE", hStyle);

  fui::TextStyle subStyle;
  subStyle.font = toybox::kTileFont;
  subStyle.align = fui::TextAlign::Center;
  subStyle.color = fui::Color::Black;

  const fui::Rect subRect = fui::makeRect(content.x, static_cast<int16_t>(titleRect.bottom() + 4), content.width, 22);
  screen.target().text(subRect, "10 Classic Games + Freeform Sandbox", subStyle);

  const int16_t cardY = static_cast<int16_t>(subRect.bottom() + 16);
  const int16_t cardH = static_cast<int16_t>(content.bottom() - cardY - 8);
  if (cardH >= 120) {
    const fui::Rect card = fui::makeRect(content.x, cardY, content.width, cardH);
    screen.target().stroke(card, fui::Paint::solid(fui::Color::Black), 2, 8);

    const fui::Rect cardHeader = fui::makeRect(card.x, static_cast<int16_t>(card.y + 14), card.width, 26);
    screen.target().text(cardHeader, "TABLETOP SUITE FEATURES", hStyle);

    screen.target().fill(
        fui::makeRect(static_cast<int16_t>(card.x + 20), static_cast<int16_t>(cardHeader.bottom() + 8),
                      static_cast<int16_t>(card.width - 40), 1),
        fui::Paint::solid(fui::Color::Black));

    fui::TextStyle bStyle;
    bStyle.font = toybox::kTileFont;
    bStyle.align = fui::TextAlign::Left;
    bStyle.color = fui::Color::Black;

    const char* const notes[] = {
        "- 10 classic board & dice games + sandbox",
        "- Solo play with AI & 2-player pass & play",
        "- Wireless Nearby Link for 2 devices",
        "- Board flipping for comfortable facing play",
        "- Automatic progress save to SD storage",
        "- Freeform Sandbox with cards, dice & tokens",
        "- Tap or press Confirm to launch a game",
        "- Use side keys or page buttons to browse",
    };
    constexpr int kNoteCount = sizeof(notes) / sizeof(notes[0]);
    int16_t textY = static_cast<int16_t>(cardHeader.bottom() + 18);
    for (int n = 0; n < kNoteCount && textY + 24 <= card.bottom(); ++n) {
      screen.target().text(
          fui::makeRect(static_cast<int16_t>(card.x + 20), textY, static_cast<int16_t>(card.width - 40), 22),
          notes[n], bStyle);
      textY = static_cast<int16_t>(textY + 28);
    }
  }
}

void buildPlaceholder(toybox::Screen& screen, const char* title, const char* message) {
  toyboxChrome(screen, title);

  const fui::Rect content = screen.contentRect();
  fui::TextStyle titleStyle;
  titleStyle.font = toybox::kUiFont;
  titleStyle.align = fui::TextAlign::Center;
  titleStyle.color = fui::Color::Black;

  fui::TextStyle bodyStyle;
  bodyStyle.font = toybox::kTileFont;
  bodyStyle.align = fui::TextAlign::Center;
  bodyStyle.color = fui::Color::Black;

  int16_t y = static_cast<int16_t>(content.y + 60);
  const char* cur = message;
  bool first = true;
  while (*cur) {
    const char* nl = std::strchr(cur, '\n');
    char lineBuf[128];
    if (nl) {
      size_t len = nl - cur;
      if (len >= sizeof(lineBuf)) len = sizeof(lineBuf) - 1;
      std::memcpy(lineBuf, cur, len);
      lineBuf[len] = '\0';
      cur = nl + 1;
    } else {
      std::snprintf(lineBuf, sizeof(lineBuf), "%s", cur);
      cur += std::strlen(cur);
    }
    const fui::Rect lineRect = fui::makeRect(content.x, y, content.width, 36);
    screen.target().text(lineRect, lineBuf, first ? titleStyle : bodyStyle);
    first = false;
    y += 44;
  }

  fui::ButtonProps btn;
  btn.label = "BACK TO LOBBY";
  btn.action = ActionBackToLobby;
  screen.button(btn, screen.takeBottom(toybox::kPillHeight));
}

}  // namespace tabletopui
