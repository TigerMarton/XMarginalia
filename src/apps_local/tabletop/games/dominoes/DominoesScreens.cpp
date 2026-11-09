#include "DominoesScreens.h"

#include <cstdio>

namespace dominoesui {

int8_t handIdxAt(int16_t tapX, int16_t tapY, int handCount) {
  if (handCount <= 0) return -1;

  if (handCount <= 7) {
    if (tapY < kHandAreaY || tapY >= kHandAreaY + kTileHeight) return -1;
    int16_t startX = static_cast<int16_t>((480 - handCount * 54) / 2);
    if (tapX < startX) return -1;
    int16_t idx = static_cast<int16_t>((tapX - startX) / 54);
    if (idx >= 0 && idx < handCount && (tapX - startX - idx * 54) <= kTileWidth) {
      return static_cast<int8_t>(idx);
    }
  } else {
    // 2 rows
    if (tapY >= kHandAreaY && tapY < kHandAreaY + 74) {
      // Row 1 (first 7)
      int16_t startX = static_cast<int16_t>((480 - 7 * 54) / 2);
      if (tapX < startX) return -1;
      int16_t idx = static_cast<int16_t>((tapX - startX) / 54);
      if (idx >= 0 && idx < 7 && (tapX - startX - idx * 54) <= kTileWidth) {
        return static_cast<int8_t>(idx);
      }
    } else if (tapY >= kHandAreaY + 80 && tapY < kHandAreaY + 154) {
      // Row 2 (remaining)
      int rem = handCount - 7;
      int16_t startX = static_cast<int16_t>((480 - rem * 54) / 2);
      if (tapX < startX) return -1;
      int16_t idx = static_cast<int16_t>((tapX - startX) / 54);
      if (idx >= 0 && idx < rem && (tapX - startX - idx * 54) <= kTileWidth) {
        return static_cast<int8_t>(7 + idx);
      }
    }
  }
  return -1;
}

namespace {

void drawPipsInSquare(toybox::Screen& screen, int16_t sx, int16_t sy, int16_t size, int pips) {
  int16_t midX = static_cast<int16_t>(sx + size / 2);
  int16_t midY = static_cast<int16_t>(sy + size / 2);
  int16_t offset = static_cast<int16_t>(size / 4);

  int16_t x0 = static_cast<int16_t>(midX - offset);
  int16_t x1 = static_cast<int16_t>(midX + offset);
  int16_t y0 = static_cast<int16_t>(midY - offset);
  int16_t y1 = static_cast<int16_t>(midY + offset);

  auto drawDot = [&](int16_t px, int16_t py) {
    screen.target().fill(fui::makeRect(static_cast<int16_t>(px - 2), static_cast<int16_t>(py - 2), 4, 4),
                         fui::Paint::solid(fui::Color::Black));
  };

  switch (pips) {
    case 1:
      drawDot(midX, midY);
      break;
    case 2:
      drawDot(x0, y0);
      drawDot(x1, y1);
      break;
    case 3:
      drawDot(x0, y0);
      drawDot(midX, midY);
      drawDot(x1, y1);
      break;
    case 4:
      drawDot(x0, y0);
      drawDot(x1, y0);
      drawDot(x0, y1);
      drawDot(x1, y1);
      break;
    case 5:
      drawDot(x0, y0);
      drawDot(x1, y0);
      drawDot(midX, midY);
      drawDot(x0, y1);
      drawDot(x1, y1);
      break;
    case 6:
      drawDot(x0, y0);
      drawDot(x0, midY);
      drawDot(x0, y1);
      drawDot(x1, y0);
      drawDot(x1, midY);
      drawDot(x1, y1);
      break;
    default:
      break;
  }
}

void drawDominoVertical(toybox::Screen& screen, int16_t x, int16_t y, int16_t w, int16_t h, int p0, int p1,
                        bool isSelected, bool isConcealed) {
  const fui::Rect tileBox = fui::makeRect(x, y, w, h);

  if (isConcealed) {
    // Secret back pattern
    screen.target().fill(tileBox, fui::Paint::solid(fui::Color::White));
    screen.target().stroke(tileBox, fui::Paint::solid(fui::Color::Black), 2);
    // Draw crosshatch icon in center
    screen.target().stroke(fui::makeRect(static_cast<int16_t>(x + 8), static_cast<int16_t>(y + 8),
                                         static_cast<int16_t>(w - 16), static_cast<int16_t>(h - 16)),
                           fui::Paint::solid(fui::Color::Black), 1);
    fui::TextStyle qStyle;
    qStyle.font = toybox::kDisplayFont;
    qStyle.color = fui::Color::Black;
    qStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(x, static_cast<int16_t>(y + h / 2 - 14), w, 28), "?", qStyle);
    return;
  }

  screen.target().fill(tileBox, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(tileBox, fui::Paint::solid(fui::Color::Black), isSelected ? 3 : 2);

  int16_t halfH = static_cast<int16_t>(h / 2);
  // Divider line
  screen.target().stroke(fui::makeRect(x, static_cast<int16_t>(y + halfH), w, 1),
                         fui::Paint::solid(fui::Color::Black), 1);

  // Top half pips
  drawPipsInSquare(screen, x, y, w, p0);
  // Bottom half pips
  drawPipsInSquare(screen, x, static_cast<int16_t>(y + halfH), w, p1);
}

void drawDominoHorizontal(toybox::Screen& screen, int16_t x, int16_t y, int16_t w, int16_t h, int p0, int p1) {
  const fui::Rect tileBox = fui::makeRect(x, y, w, h);
  screen.target().fill(tileBox, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(tileBox, fui::Paint::solid(fui::Color::Black), 2);

  int16_t halfW = static_cast<int16_t>(w / 2);
  // Divider line
  screen.target().stroke(fui::makeRect(static_cast<int16_t>(x + halfW), y, 1, h),
                         fui::Paint::solid(fui::Color::Black), 1);

  // Left half pips
  drawPipsInSquare(screen, x, y, h, p0);
  // Right half pips
  drawPipsInSquare(screen, static_cast<int16_t>(x + halfW), y, h, p1);
}

}  // namespace

void toyboxChrome(toybox::Screen& screen, const char* title, const char* rightLabel = nullptr) {
  fui::HeaderProps header;
  header.title = title;
  header.rightLabel = rightLabel;
  header.subtitleText = fui::TextStyle{};
  header.subtitleText.font = toybox::kTileFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
}

void render(toybox::Screen& screen, const dominoes::State& state, const uint8_t humanPlayer) {
  const int pIdx = (humanPlayer > 0) ? 1 : 0;
  const int oppIdx = 1 - pIdx;

  // 1. Header
  const char* modeLabel = (state.mode == dominoes::Mode::Nearby)
                              ? "NEARBY LINK"
                              : ((state.mode == dominoes::Mode::Solo) ? "SOLO VS AI" : "PASS & PLAY");
  toyboxChrome(screen, "DOMINOES", modeLabel);

  // 2. Status Banner
  fui::TextStyle headlineStyle;
  headlineStyle.font = toybox::kUiFont;
  headlineStyle.color = fui::Color::Black;
  headlineStyle.align = fui::TextAlign::Center;

  fui::TextStyle sublineStyle;
  sublineStyle.font = toybox::kTileFont;
  sublineStyle.color = fui::Color::Black;
  sublineStyle.align = fui::TextAlign::Center;

  char headline[64];
  char subline[64];

  if (state.roundOver) {
    if (state.winner == static_cast<dominoes::Player>(pIdx)) {
      std::snprintf(headline, sizeof(headline), "DOMINO! YOU WIN!");
    } else if (state.winner == static_cast<dominoes::Player>(oppIdx)) {
      std::snprintf(headline, sizeof(headline),
                    (state.mode == dominoes::Mode::Solo)
                        ? "AI WINS!"
                        : ((state.mode == dominoes::Mode::Nearby) ? "OPPONENT WINS!" : "PLAYER 2 WINS!"));
    } else {
      std::snprintf(headline, sizeof(headline), "BLOCKED GAME (DRAW)");
    }
    std::snprintf(subline, sizeof(subline), "Score: YOU %d  |  THEM %d", state.score[pIdx], state.score[oppIdx]);
  } else {
    if (state.turn == static_cast<dominoes::Player>(pIdx)) {
      std::snprintf(headline, sizeof(headline), "%s",
                    (state.mode == dominoes::Mode::Solo || state.mode == dominoes::Mode::Nearby)
                        ? "YOUR TURN"
                        : (pIdx == 0 ? "PLAYER 1'S TURN" : "PLAYER 2'S TURN"));
    } else {
      std::snprintf(headline, sizeof(headline), "%s",
                    (state.mode == dominoes::Mode::Solo)
                        ? "AI THINKING..."
                        : ((state.mode == dominoes::Mode::Nearby)
                               ? "OPPONENT'S TURN"
                               : (oppIdx == 0 ? "PLAYER 1'S TURN" : "PLAYER 2'S TURN")));
    }

    std::snprintf(subline, sizeof(subline), "BONEYARD: %d TILES  |  OPPONENT: %d TILES",
                  state.boneyardCount, state.handCount[oppIdx]);
  }

  screen.target().text(fui::makeRect(16, 88, 448, 22), headline, headlineStyle);
  screen.target().text(fui::makeRect(16, 124, 448, 18), subline, sublineStyle);

  // Open Ends Pill
  char endsBuf[64];
  std::snprintf(endsBuf, sizeof(endsBuf), "[ < LEFT: %d ]     ---     [ RIGHT: %d > ]",
                state.leftEnd, state.rightEnd);
  fui::TextStyle endsStyle;
  endsStyle.font = toybox::kTileFont;
  endsStyle.color = fui::Color::Black;
  endsStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(16, 150, 448, 18), endsBuf, endsStyle);

  // 3. Table Train Area (y = 176 .. 362)
  const fui::Rect trainArea = fui::makeRect(16, 176, 448, 186);
  screen.target().stroke(trainArea, fui::Paint::solid(fui::Color::Black), 1);

  // Draw Train Tiles
  if (state.trainCount > 0) {
    if (state.trainCount <= 8) {
      // 1 centered row of horizontal dominoes
      int16_t tW = 48;
      int16_t tH = 26;
      int16_t totalW = static_cast<int16_t>(state.trainCount * (tW + 4) - 4);
      int16_t startX = static_cast<int16_t>((480 - totalW) / 2);
      int16_t trainY = static_cast<int16_t>(trainArea.y + trainArea.height / 2 - tH / 2);

      for (int i = 0; i < state.trainCount; ++i) {
        int16_t tx = static_cast<int16_t>(startX + i * (tW + 4));
        drawDominoHorizontal(screen, tx, trainY, tW, tH, state.train[i].pips[0], state.train[i].pips[1]);
      }
    } else {
      // Multi-tile snaking layout: show first 4 and last 4 with ellipsis
      int16_t tW = 44;
      int16_t tH = 24;

      // Row 1: Leftmost 4 tiles
      int16_t trainY1 = static_cast<int16_t>(trainArea.y + 28);
      int16_t startX1 = 32;
      for (int i = 0; i < 4; ++i) {
        int16_t tx = static_cast<int16_t>(startX1 + i * (tW + 4));
        drawDominoHorizontal(screen, tx, trainY1, tW, tH, state.train[i].pips[0], state.train[i].pips[1]);
      }

      // Middle ellipsis
      screen.target().text(fui::makeRect(startX1 + 4 * (tW + 4) + 12, trainY1 + 4, 60, 20),
                           ". . . . .", endsStyle);

      // Row 2: Rightmost 4 tiles
      int16_t trainY2 = static_cast<int16_t>(trainArea.y + 104);
      int16_t startX2 = static_cast<int16_t>(480 - 32 - 4 * (tW + 4));
      int startIdx = state.trainCount - 4;
      for (int i = 0; i < 4; ++i) {
        int16_t tx = static_cast<int16_t>(startX2 + i * (tW + 4));
        drawDominoHorizontal(screen, tx, trainY2, tW, tH, state.train[startIdx + i].pips[0],
                             state.train[startIdx + i].pips[1]);
      }
    }
  }

  // 4. Player Hand (y = 360 .. 560)
  int hCount = state.handCount[pIdx];
  fui::TextStyle handHeaderStyle;
  handHeaderStyle.font = toybox::kTileFont;
  handHeaderStyle.color = fui::Color::Black;
  handHeaderStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(16, 370, 448, 18), "YOUR HAND (TAP TILE TO SELECT):", handHeaderStyle);

  if (hCount <= 7) {
    int16_t startX = static_cast<int16_t>((480 - hCount * 54) / 2);
    for (int i = 0; i < hCount; ++i) {
      int8_t tIdx = state.hand[pIdx][i];
      int16_t tx = static_cast<int16_t>(startX + i * 54);
      bool isSelected = (state.selectedHandIdx == i);
      drawDominoVertical(screen, tx, kHandAreaY, kTileWidth, kTileHeight,
                         dominoes::kTiles[tIdx].pips[0], dominoes::kTiles[tIdx].pips[1],
                         isSelected, state.privacyConcealed);
    }
  } else {
    // 2 rows for large hands
    int16_t startX1 = static_cast<int16_t>((480 - 7 * 54) / 2);
    for (int i = 0; i < 7; ++i) {
      int8_t tIdx = state.hand[pIdx][i];
      int16_t tx = static_cast<int16_t>(startX1 + i * 54);
      bool isSelected = (state.selectedHandIdx == i);
      drawDominoVertical(screen, tx, kHandAreaY, kTileWidth, 70,
                         dominoes::kTiles[tIdx].pips[0], dominoes::kTiles[tIdx].pips[1],
                         isSelected, state.privacyConcealed);
    }
    int rem = hCount - 7;
    int16_t startX2 = static_cast<int16_t>((480 - rem * 54) / 2);
    for (int i = 0; i < rem; ++i) {
      int8_t tIdx = state.hand[pIdx][7 + i];
      int16_t tx = static_cast<int16_t>(startX2 + i * 54);
      bool isSelected = (state.selectedHandIdx == (7 + i));
      drawDominoVertical(screen, tx, static_cast<int16_t>(kHandAreaY + 76), kTileWidth, 70,
                         dominoes::kTiles[tIdx].pips[0], dominoes::kTiles[tIdx].pips[1],
                         isSelected, state.privacyConcealed);
    }
  }

  // 5. Contextual Action Buttons
  const int16_t row0Y = 590;

  if (state.selectedHandIdx != -1) {
    int8_t selTile = state.hand[pIdx][state.selectedHandIdx];
    bool canL = dominoes::canPlayLeft(state, selTile);
    bool canR = dominoes::canPlayRight(state, selTile);

    fui::ButtonProps leftBtn;
    leftBtn.label = "PLAY LEFT";
    leftBtn.action = canL ? ActionPlayLeft : fui::NO_ACTION;
    screen.button(leftBtn, fui::makeRect(16, row0Y, 216, 44));

    fui::ButtonProps rightBtn;
    rightBtn.label = "PLAY RIGHT";
    rightBtn.action = canR ? ActionPlayRight : fui::NO_ACTION;
    screen.button(rightBtn, fui::makeRect(248, row0Y, 216, 44));
  } else {
    bool hasPlay = dominoes::hasAnyLegalPlay(state, static_cast<dominoes::Player>(pIdx));
    if (!hasPlay && state.boneyardCount > 0) {
      char drawLabel[32];
      std::snprintf(drawLabel, sizeof(drawLabel), "DRAW TILE (%d)", state.boneyardCount);
      fui::ButtonProps drawBtn;
      drawBtn.label = drawLabel;
      drawBtn.action = ActionDrawTile;
      screen.button(drawBtn, fui::makeRect(16, row0Y, 216, 44));
    } else if (!hasPlay && state.boneyardCount == 0) {
      fui::ButtonProps passBtn;
      passBtn.label = "PASS TURN";
      passBtn.action = ActionPassTurn;
      screen.button(passBtn, fui::makeRect(16, row0Y, 216, 44));
    } else {
      fui::ButtonProps hintBtn;
      hintBtn.label = "SELECT TILE";
      hintBtn.action = fui::NO_ACTION;
      screen.button(hintBtn, fui::makeRect(16, row0Y, 216, 44));
    }

    fui::ButtonProps privBtn;
    privBtn.label = state.privacyConcealed ? "PEEK HAND" : "CONCEAL";
    privBtn.action = ActionTogglePrivacy;
    screen.button(privBtn, fui::makeRect(248, row0Y, 216, 44));
  }

  // Row 1: MODE & RESTART
  const int16_t row1Y = 660;
  char modeBtnText[32];
  std::snprintf(modeBtnText, sizeof(modeBtnText), "MODE: %s",
                (state.mode == dominoes::Mode::Nearby) ? "NEARBY"
                                                       : ((state.mode == dominoes::Mode::Solo) ? "SOLO" : "2P"));
  fui::ButtonProps modeBtn;
  modeBtn.label = modeBtnText;
  modeBtn.action = ActionToggleMode;
  screen.button(modeBtn, fui::makeRect(16, row1Y, 216, 44));

  fui::ButtonProps rstBtn;
  rstBtn.label = "NEW GAME";
  rstBtn.action = ActionRestart;
  screen.button(rstBtn, fui::makeRect(248, row1Y, 216, 44));

  // Row 2: LOBBY
  const int16_t row2Y = 716;
  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "BACK TO LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, fui::makeRect(16, row2Y, 448, 44));
}

}  // namespace dominoesui
