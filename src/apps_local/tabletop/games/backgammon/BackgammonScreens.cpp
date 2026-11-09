#include "BackgammonScreens.h"

#include <cstdio>
#include <cstring>

#include "../../../ui/ToyboxFormat.h"

namespace backgammonui {

namespace {

constexpr int16_t kBoardLeft = 12;
constexpr int16_t kBoardTop = 124;
constexpr int16_t kBoardWidth = 456;
constexpr int16_t kBoardHeight = 460;

constexpr int16_t kPointWidth = 32;
constexpr int16_t kTriangleHeight = 160;
constexpr int16_t kBarWidth = 32;
constexpr int16_t kBearOffWidth = 40;

constexpr int16_t kLeftBoardLeft = kBoardLeft;
constexpr int16_t kBarLeft = kLeftBoardLeft + 6 * kPointWidth;  // 12 + 192 = 204
constexpr int16_t kRightBoardLeft = kBarLeft + kBarWidth;       // 204 + 32 = 236
constexpr int16_t kBearOffLeft = kRightBoardLeft + 6 * kPointWidth;  // 236 + 192 = 428

constexpr int16_t kBoardMidY = kBoardTop + kBoardHeight / 2;    // 124 + 230 = 354
constexpr int16_t kCheckerRadius = 12;

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
  screen.insetContent(fui::Insets{toybox::kGutter, toybox::kMargin, toybox::kMargin, toybox::kMargin});
}

// Map column (0..11) and top/bottom row to point index (0..23)
int8_t colRowToPoint(const int col, const bool isTop, const bool flip) {
  // cols: 0..5 (left board), 6..11 (right board)
  if (!flip) {
    if (isTop) {
      // Points 12..23 (left to right)
      return static_cast<int8_t>(12 + col);
    } else {
      // Points 11..0 (left to right)
      return static_cast<int8_t>(11 - col);
    }
  } else {
    // 180 degree rotation
    if (isTop) {
      // Points 0..11 (left to right)
      return static_cast<int8_t>(col);
    } else {
      // Points 23..12 (left to right)
      return static_cast<int8_t>(23 - col);
    }
  }
}

int16_t colToX(const int col) {
  if (col < 6) {
    return static_cast<int16_t>(kLeftBoardLeft + col * kPointWidth);
  } else {
    return static_cast<int16_t>(kRightBoardLeft + (col - 6) * kPointWidth);
  }
}

void drawTriangleHatch(toybox::Screen& screen, const int16_t xBase, const int16_t yBase, const bool isTop) {
  const int16_t xMid = static_cast<int16_t>(xBase + kPointWidth / 2);
  const int16_t step = isTop ? 4 : -4;
  const int count = kTriangleHeight / 4;

  for (int i = 1; i < count; ++i) {
    const int16_t y = static_cast<int16_t>(yBase + i * step);
    const int16_t span = static_cast<int16_t>((kPointWidth * (kTriangleHeight - i * 4)) / (2 * kTriangleHeight));
    screen.target().line(fui::Point{static_cast<int16_t>(xMid - span), y},
                         fui::Point{static_cast<int16_t>(xMid + span), y},
                         1, fui::Paint::solid(fui::Color::Black));
  }
}

void drawDie(toybox::Screen& screen, const int16_t x, const int16_t y, const uint8_t value, const bool active) {
  const fui::Rect box = fui::makeRect(x, y, 32, 32);
  screen.target().fill(box, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), active ? 2 : 1, 4);

  // Draw pips (radius 3)
  const int16_t cx = static_cast<int16_t>(x + 16);
  const int16_t cy = static_cast<int16_t>(y + 16);
  const int16_t left = static_cast<int16_t>(x + 8);
  const int16_t right = static_cast<int16_t>(x + 24);
  const int16_t top = static_cast<int16_t>(y + 8);
  const int16_t bottom = static_cast<int16_t>(y + 24);

  const fui::Color pipCol = active ? fui::Color::Black : fui::Color::Black;

  if (value == 1 || value == 3 || value == 5) {
    toybox::disc(screen, cx, cy, 3, pipCol);
  }
  if (value >= 2) {
    toybox::disc(screen, left, top, 3, pipCol);
    toybox::disc(screen, right, bottom, 3, pipCol);
  }
  if (value >= 4) {
    toybox::disc(screen, right, top, 3, pipCol);
    toybox::disc(screen, left, bottom, 3, pipCol);
  }
  if (value == 6) {
    toybox::disc(screen, left, cy, 3, pipCol);
    toybox::disc(screen, right, cy, 3, pipCol);
  }

  if (!active) {
    // Cross out used die
    screen.target().line(fui::Point{x, y}, fui::Point{static_cast<int16_t>(x + 32), static_cast<int16_t>(y + 32)},
                         1, fui::Paint::solid(fui::Color::Black));
  }
}

void drawChecker(toybox::Screen& screen, const int16_t cx, const int16_t cy,
                 const backgammon::Player owner, const int badgeCount = 0) {
  if (owner == backgammon::Player::White) {
    // White checker: White disc with 2px Black ring
    toybox::ring(screen, cx, cy, kCheckerRadius, 2, fui::Color::Black, fui::Color::White);
    if (badgeCount > 0) {
      char bBuf[16];
      std::snprintf(bBuf, sizeof(bBuf), "%d", badgeCount);
      fui::TextStyle style;
      style.font = toybox::kTileFont;
      style.color = fui::Color::Black;
      style.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(static_cast<int16_t>(cx - 10), static_cast<int16_t>(cy - 9), 20, 18),
                           bBuf, style);
    }
  } else {
    // Black checker: Solid Black disc with 1px White inner ring
    toybox::disc(screen, cx, cy, kCheckerRadius, fui::Color::Black);
    toybox::ring(screen, cx, cy, static_cast<int16_t>(kCheckerRadius - 2), 1, fui::Color::White, fui::Color::Black);
    if (badgeCount > 0) {
      char bBuf[16];
      std::snprintf(bBuf, sizeof(bBuf), "%d", badgeCount);
      fui::TextStyle style;
      style.font = toybox::kTileFont;
      style.color = fui::Color::White;
      style.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(static_cast<int16_t>(cx - 10), static_cast<int16_t>(cy - 9), 20, 18),
                           bBuf, style);
    }
  }
}

}  // namespace

fui::Rect boardRect() {
  return fui::makeRect(kBoardLeft, kBoardTop, kBoardWidth, kBoardHeight);
}

fui::Rect barRect() {
  return fui::makeRect(kBarLeft, kBoardTop, kBarWidth, kBoardHeight);
}

fui::Rect bearOffRect(const backgammon::Player p, const bool flip) {
  const bool atTop = (!flip && p == backgammon::Player::Black) || (flip && p == backgammon::Player::White);
  const int16_t y = atTop ? kBoardTop : kBoardMidY;
  const int16_t h = static_cast<int16_t>(kBoardHeight / 2);
  return fui::makeRect(kBearOffLeft, y, kBearOffWidth, h);
}

int8_t pointAt(const int x, const int y, const bool flip) {
  if (x < kBoardLeft || x >= kBoardLeft + kBoardWidth || y < kBoardTop || y >= kBoardTop + kBoardHeight) {
    return -99;
  }

  // Bar check
  if (x >= kBarLeft && x < kBarLeft + kBarWidth) {
    return backgammon::kFromBar;
  }

  // Bear-off check
  if (x >= kBearOffLeft && x < kBearOffLeft + kBearOffWidth) {
    return backgammon::kToOff;
  }

  // Point columns check
  int col = -1;
  if (x >= kLeftBoardLeft && x < kLeftBoardLeft + 6 * kPointWidth) {
    col = (x - kLeftBoardLeft) / kPointWidth;
  } else if (x >= kRightBoardLeft && x < kRightBoardLeft + 6 * kPointWidth) {
    col = 6 + (x - kRightBoardLeft) / kPointWidth;
  }

  if (col < 0 || col >= 12) return -99;

  const bool isTop = (y < kBoardMidY);
  return colRowToPoint(col, isTop, flip);
}

void buildBoard(toybox::Screen& screen, const ScreenModel& model) {
  const char* modeLabel = (model.mode == PlayMode::Nearby)
                              ? "NEARBY LINK"
                              : ((model.mode == PlayMode::SoloVsAI) ? "SOLO VS AI" : "PASS & PLAY");
  toyboxChrome(screen, "BACKGAMMON", modeLabel);

  // Subtitle / Banner area
  char statusBuf[64];
  if (model.game.isOver) {
    if (model.mode == PlayMode::Nearby) {
      const bool won = model.game.winner == model.humanPlayer;
      const char* mult = (model.game.winMultiplier == 3)   ? " (BACKGAMMON)"
                         : (model.game.winMultiplier == 2) ? " (GAMMON)"
                                                           : "";
      std::snprintf(statusBuf, sizeof(statusBuf), "%s%s!", won ? "YOU WIN" : "OPPONENT WINS", mult);
    } else {
      const char* winP = (model.game.winner == backgammon::Player::White) ? "WHITE" : "BLACK";
      const char* mult = (model.game.winMultiplier == 3)   ? " (BACKGAMMON)"
                         : (model.game.winMultiplier == 2) ? " (GAMMON)"
                                                           : "";
      std::snprintf(statusBuf, sizeof(statusBuf), "%s WINS%s!", winP, mult);
    }
  } else if (model.isAiThinking) {
    std::snprintf(statusBuf, sizeof(statusBuf), "AI IS THINKING...");
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    const int whitePips = backgammon::calculatePipCount(model.game, backgammon::Player::White);
    const int blackPips = backgammon::calculatePipCount(model.game, backgammon::Player::Black);
    std::snprintf(statusBuf, sizeof(statusBuf), "%s  |  PIPS: W %d  B %d",
                  yourTurn ? "YOUR TURN" : "OPPONENT'S TURN", whitePips, blackPips);
  } else {
    const char* curP = (model.game.turn == backgammon::Player::White) ? "WHITE" : "BLACK";
    const int whitePips = backgammon::calculatePipCount(model.game, backgammon::Player::White);
    const int blackPips = backgammon::calculatePipCount(model.game, backgammon::Player::Black);
    std::snprintf(statusBuf, sizeof(statusBuf), "%s'S TURN  |  PIPS: W %d  B %d", curP, whitePips, blackPips);
  }

  fui::TextStyle statusStyle;
  statusStyle.font = toybox::kTileFont;
  statusStyle.color = fui::Color::Black;
  statusStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(16, 88, 448, 24), statusBuf, statusStyle);

  // 1. Board Outer Frame
  const fui::Rect bRect = boardRect();
  screen.target().fill(bRect, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(bRect, fui::Paint::solid(fui::Color::Black), 2);

  // 2. Bar Divider
  const fui::Rect bar = barRect();
  screen.target().stroke(bar, fui::Paint::solid(fui::Color::Black), 1);

  // Bar label
  fui::TextStyle barStyle;
  barStyle.font = toybox::kTileFont;
  barStyle.color = fui::Color::Black;
  barStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(bar.x, static_cast<int16_t>(kBoardMidY - 10), bar.width, 20),
                       "BAR", barStyle);

  // 3. Bear-Off Trays
  const fui::Rect bOff = fui::makeRect(kBearOffLeft, kBoardTop, kBearOffWidth, kBoardHeight);
  screen.target().stroke(bOff, fui::Paint::solid(fui::Color::Black), 1);
  screen.target().line(fui::Point{kBearOffLeft, kBoardMidY},
                       fui::Point{static_cast<int16_t>(kBearOffLeft + kBearOffWidth), kBoardMidY},
                       1, fui::Paint::solid(fui::Color::Black));

  // Bear-off text
  char offBuf[16];
  const uint8_t topOff = (!model.flipBoard) ? model.game.borneOff[1] : model.game.borneOff[0];
  const uint8_t botOff = (!model.flipBoard) ? model.game.borneOff[0] : model.game.borneOff[1];
  const char* topTag = (!model.flipBoard) ? "B" : "W";
  const char* botTag = (!model.flipBoard) ? "W" : "B";

  std::snprintf(offBuf, sizeof(offBuf), "%s:%d", topTag, topOff);
  screen.target().text(fui::makeRect(kBearOffLeft, static_cast<int16_t>(kBoardTop + 8), kBearOffWidth, 20),
                       offBuf, barStyle);

  std::snprintf(offBuf, sizeof(offBuf), "%s:%d", botTag, botOff);
  screen.target().text(fui::makeRect(kBearOffLeft, static_cast<int16_t>(kBoardMidY + 8), kBearOffWidth, 20),
                       offBuf, barStyle);

  // 4. Draw Triangles for each column
  for (int col = 0; col < 12; ++col) {
    const int16_t xCol = colToX(col);
    const int8_t ptTop = colRowToPoint(col, true, model.flipBoard);
    const int8_t ptBot = colRowToPoint(col, false, model.flipBoard);

    // Top Triangle
    const int16_t xMid = static_cast<int16_t>(xCol + kPointWidth / 2);
    const int16_t yApexTop = static_cast<int16_t>(kBoardTop + kTriangleHeight);
    screen.target().line(fui::Point{xCol, kBoardTop}, fui::Point{xMid, yApexTop}, 1,
                         fui::Paint::solid(fui::Color::Black));
    screen.target().line(fui::Point{static_cast<int16_t>(xCol + kPointWidth), kBoardTop},
                         fui::Point{xMid, yApexTop}, 1, fui::Paint::solid(fui::Color::Black));

    if (ptTop % 2 == 1) {
      drawTriangleHatch(screen, xCol, kBoardTop, true);
    }

    // Bottom Triangle
    const int16_t yBaseBot = static_cast<int16_t>(kBoardTop + kBoardHeight);
    const int16_t yApexBot = static_cast<int16_t>(yBaseBot - kTriangleHeight);
    screen.target().line(fui::Point{xCol, yBaseBot}, fui::Point{xMid, yApexBot}, 1,
                         fui::Paint::solid(fui::Color::Black));
    screen.target().line(fui::Point{static_cast<int16_t>(xCol + kPointWidth), yBaseBot},
                         fui::Point{xMid, yApexBot}, 1, fui::Paint::solid(fui::Color::Black));

    if (ptBot % 2 == 1) {
      drawTriangleHatch(screen, xCol, yBaseBot, false);
    }
  }

  // 5. Draw Checkers on Points
  for (int col = 0; col < 12; ++col) {
    const int16_t xCol = colToX(col);
    const int16_t cx = static_cast<int16_t>(xCol + kPointWidth / 2);

    // Top point
    const int8_t ptTop = colRowToPoint(col, true, model.flipBoard);
    const int countTop = model.game.board[ptTop];
    if (countTop != 0) {
      const backgammon::Player pOwner = (countTop > 0) ? backgammon::Player::White : backgammon::Player::Black;
      const int absCount = (countTop > 0) ? countTop : -countTop;
      const int drawCount = (absCount > 5) ? 5 : absCount;

      for (int i = 0; i < drawCount; ++i) {
        const int16_t cy = static_cast<int16_t>(kBoardTop + 16 + i * 26);
        const int badge = (i == 4 && absCount > 5) ? absCount : 0;
        drawChecker(screen, cx, cy, pOwner, badge);
      }
    }

    // Bottom point
    const int8_t ptBot = colRowToPoint(col, false, model.flipBoard);
    const int countBot = model.game.board[ptBot];
    if (countBot != 0) {
      const backgammon::Player pOwner = (countBot > 0) ? backgammon::Player::White : backgammon::Player::Black;
      const int absCount = (countBot > 0) ? countBot : -countBot;
      const int drawCount = (absCount > 5) ? 5 : absCount;

      for (int i = 0; i < drawCount; ++i) {
        const int16_t cy = static_cast<int16_t>(kBoardTop + kBoardHeight - 16 - i * 26);
        const int badge = (i == 4 && absCount > 5) ? absCount : 0;
        drawChecker(screen, cx, cy, pOwner, badge);
      }
    }
  }

  // 6. Draw Checkers on Bar
  const int16_t barCx = static_cast<int16_t>(kBarLeft + kBarWidth / 2);
  const int8_t topBarCount = (!model.flipBoard) ? model.game.bar[1] : model.game.bar[0];
  const backgammon::Player topBarOwner = (!model.flipBoard) ? backgammon::Player::Black : backgammon::Player::White;
  if (topBarCount > 0) {
    const int16_t cy = static_cast<int16_t>(kBoardMidY - 32);
    drawChecker(screen, barCx, cy, topBarOwner, (topBarCount > 1) ? topBarCount : 0);
  }

  const int8_t botBarCount = (!model.flipBoard) ? model.game.bar[0] : model.game.bar[1];
  const backgammon::Player botBarOwner = (!model.flipBoard) ? backgammon::Player::White : backgammon::Player::Black;
  if (botBarCount > 0) {
    const int16_t cy = static_cast<int16_t>(kBoardMidY + 32);
    drawChecker(screen, barCx, cy, botBarOwner, (botBarCount > 1) ? botBarCount : 0);
  }

  // 7. Highlights: Selected point and legal destinations
  if (model.selectedPoint != -99) {
    // Highlight selected source
    if (model.selectedPoint == backgammon::kFromBar) {
      toybox::bracket(screen, bar, 12, 3);
    } else if (model.selectedPoint >= 0 && model.selectedPoint < backgammon::kPoints) {
      // Find where selectedPoint is rendered
      for (int col = 0; col < 12; ++col) {
        if (colRowToPoint(col, true, model.flipBoard) == model.selectedPoint) {
          const int16_t xCol = colToX(col);
          toybox::bracket(screen, fui::makeRect(xCol, kBoardTop, kPointWidth, kTriangleHeight), 8, 2);
          break;
        }
        if (colRowToPoint(col, false, model.flipBoard) == model.selectedPoint) {
          const int16_t xCol = colToX(col);
          const int16_t yBase = static_cast<int16_t>(kBoardTop + kBoardHeight - kTriangleHeight);
          toybox::bracket(screen, fui::makeRect(xCol, yBase, kPointWidth, kTriangleHeight), 8, 2);
          break;
        }
      }
    }

    // Highlight legal target moves
    backgammon::Move legalMoves[64];
    const size_t nMoves = backgammon::generateLegalMoves(model.game, legalMoves, 64);
    for (size_t mi = 0; mi < nMoves; ++mi) {
      if (legalMoves[mi].from == model.selectedPoint) {
        const int8_t tgt = legalMoves[mi].to;
        if (tgt == backgammon::kToOff) {
          // Highlight bear-off tray
          toybox::bracket(screen, bearOffRect(model.game.turn, model.flipBoard), 12, 3);
        } else if (tgt >= 0 && tgt < backgammon::kPoints) {
          // Highlight target point
          for (int col = 0; col < 12; ++col) {
            if (colRowToPoint(col, true, model.flipBoard) == tgt) {
              const int16_t xCol = colToX(col);
              const int16_t cx = static_cast<int16_t>(xCol + kPointWidth / 2);
              toybox::ring(screen, cx, static_cast<int16_t>(kBoardTop + kTriangleHeight - 12), 6, 2,
                           fui::Color::Black, fui::Color::White);
              break;
            }
            if (colRowToPoint(col, false, model.flipBoard) == tgt) {
              const int16_t xCol = colToX(col);
              const int16_t cx = static_cast<int16_t>(xCol + kPointWidth / 2);
              const int16_t yApex = static_cast<int16_t>(kBoardTop + kBoardHeight - kTriangleHeight + 12);
              toybox::ring(screen, cx, yApex, 6, 2, fui::Color::Black, fui::Color::White);
              break;
            }
          }
        }
      }
    }
  }

  // 8. Dice display in middle gap (right side)
  if (model.game.isRolled) {
    const int16_t diceY = static_cast<int16_t>(kBoardMidY - 16);
    drawDie(screen, 246, diceY, model.game.dice[0], model.game.movesLeft[0] > 0);
    drawDie(screen, 286, diceY, model.game.dice[1], model.game.movesLeft[1] > 0);

    char mLeftBuf[32];
    std::snprintf(mLeftBuf, sizeof(mLeftBuf), "MOVES: %d", model.game.moveCount);
    fui::TextStyle mStyle;
    mStyle.font = toybox::kTileFont;
    mStyle.color = fui::Color::Black;
    mStyle.align = fui::TextAlign::Left;
    screen.target().text(fui::makeRect(330, diceY + 6, 94, 20), mLeftBuf, mStyle);
  }

  // 9. Primary Action Button
  const int16_t btnY = 600;
  if (model.game.isOver) {
    fui::ButtonProps btn;
    btn.label = "PLAY AGAIN";
    btn.action = ActionRestart;
    screen.button(btn, fui::makeRect(16, btnY, 448, 48));
  } else if (!model.game.isRolled) {
    fui::ButtonProps btn;
    btn.label = "ROLL DICE";
    btn.action = ActionRollDice;
    screen.button(btn, fui::makeRect(16, btnY, 448, 48));
  } else {
    // Rolled: check if bear off is legal
    bool canOff = false;
    if (model.selectedPoint != -99) {
      backgammon::Move legalMoves[64];
      const size_t nMoves = backgammon::generateLegalMoves(model.game, legalMoves, 64);
      for (size_t mi = 0; mi < nMoves; ++mi) {
        if (legalMoves[mi].from == model.selectedPoint && legalMoves[mi].to == backgammon::kToOff) {
          canOff = true;
          break;
        }
      }
    }

    if (canOff) {
      fui::ButtonProps offBtn;
      offBtn.label = "BEAR OFF";
      offBtn.action = ActionBearOff;
      screen.button(offBtn, fui::makeRect(16, btnY, 216, 48));

      fui::ButtonProps endBtn;
      endBtn.label = "END TURN";
      endBtn.action = ActionEndTurn;
      screen.button(endBtn, fui::makeRect(248, btnY, 216, 48));
    } else {
      fui::ButtonProps endBtn;
      endBtn.label = "END TURN";
      endBtn.action = ActionEndTurn;
      screen.button(endBtn, fui::makeRect(16, btnY, 448, 48));
    }
  }

  // 10. Secondary Control Buttons
  const int16_t row2Y = 660;
  fui::ButtonProps modeBtn;
  modeBtn.label = (model.mode == PlayMode::Nearby) ? "MODE: NEARBY" : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
  modeBtn.action = ActionToggleMode;
  screen.button(modeBtn, fui::makeRect(16, row2Y, 216, 44));

  fui::ButtonProps flipBtn;
  flipBtn.label = "FLIP BOARD";
  flipBtn.action = ActionFlipBoard;
  screen.button(flipBtn, fui::makeRect(248, row2Y, 216, 44));

  const int16_t row3Y = 716;
  fui::ButtonProps rstBtn;
  rstBtn.label = "RESTART";
  rstBtn.action = ActionRestart;
  screen.button(rstBtn, fui::makeRect(16, row3Y, 216, 44));

  fui::ButtonProps lbyBtn;
  lbyBtn.label = "LOBBY";
  lbyBtn.action = ActionBackToLobby;
  screen.button(lbyBtn, fui::makeRect(248, row3Y, 216, 44));
}

}  // namespace backgammonui
