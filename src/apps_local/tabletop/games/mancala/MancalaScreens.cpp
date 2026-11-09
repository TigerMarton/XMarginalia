#include "MancalaScreens.h"

#include <cstdio>
#include <cstring>

#include "../../../ui/ToyboxFormat.h"

namespace mancalaui {

namespace {

constexpr int16_t kBoardLeft = 16;
constexpr int16_t kBoardTop = 148;
constexpr int16_t kBoardWidth = 448;
constexpr int16_t kBoardHeight = 356;

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

void drawPitBowl(toybox::Screen& screen, const fui::Rect& where, const int count, const bool highlight) {
  // White interior
  screen.target().fill(where, fui::Paint::solid(fui::Color::White));
  // Border: 3px if highlight (tappable legal move), 1px hairline otherwise
  const int16_t weight = highlight ? 3 : toybox::kHairline;
  screen.target().stroke(where, fui::Paint::solid(fui::Color::Black), weight);

  // Numeric count at top of pit
  char numBuf[16];
  std::snprintf(numBuf, sizeof(numBuf), "%d", count);
  fui::TextStyle numStyle;
  numStyle.font = toybox::kUiFont;
  numStyle.color = fui::Color::Black;
  numStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(where.x, static_cast<int16_t>(where.y + 8), where.width, 28),
                       numBuf, numStyle);

  // Pebble dots in lower bowl
  const int maxPebbles = (count > 8) ? 8 : count;
  for (int s = 0; s < maxPebbles; ++s) {
    const int col = s % 2;
    const int row = s / 2;
    const int16_t sx = static_cast<int16_t>(where.x + 16 + col * 14);
    const int16_t sy = static_cast<int16_t>(where.y + 44 + row * 16);
    toybox::disc(screen, sx, sy, 4, fui::Color::Black);
  }
  if (count > 8) {
    fui::TextStyle plusStyle;
    plusStyle.font = toybox::kTileFont;
    plusStyle.color = fui::Color::Black;
    plusStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(where.x, static_cast<int16_t>(where.bottom() - 24), where.width, 18),
                         "+", plusStyle);
  }
}

void drawStore(toybox::Screen& screen, const fui::Rect& where, const char* label, const int count) {
  screen.target().fill(where, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(where, fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  // Label at top
  fui::TextStyle labelStyle;
  labelStyle.font = toybox::kTileFont;
  labelStyle.color = fui::Color::Black;
  labelStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(where.x, static_cast<int16_t>(where.y + 12), where.width, 22),
                       label, labelStyle);

  // Big count in middle
  char numBuf[16];
  std::snprintf(numBuf, sizeof(numBuf), "%d", count);
  fui::TextStyle numStyle;
  numStyle.font = toybox::kDisplayFont;
  numStyle.color = fui::Color::Black;
  numStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(where.x, static_cast<int16_t>(where.y + 46), where.width, 36),
                       numBuf, numStyle);

  // Pebbles in lower portion
  const int maxPebbles = (count > 16) ? 16 : count;
  for (int s = 0; s < maxPebbles; ++s) {
    const int col = s % 2;
    const int row = s / 2;
    const int16_t sx = static_cast<int16_t>(where.x + 16 + col * 14);
    const int16_t sy = static_cast<int16_t>(where.y + 100 + row * 16);
    toybox::disc(screen, sx, sy, 4, fui::Color::Black);
  }
  if (count > 16) {
    fui::TextStyle plusStyle;
    plusStyle.font = toybox::kTileFont;
    plusStyle.color = fui::Color::Black;
    plusStyle.align = fui::TextAlign::Center;
    screen.target().text(fui::makeRect(where.x, static_cast<int16_t>(where.bottom() - 26), where.width, 20),
                         "+", plusStyle);
  }
}

}  // namespace

fui::Rect boardRect() {
  return fui::makeRect(kBoardLeft, kBoardTop, kBoardWidth, kBoardHeight);
}

fui::Rect pitRect(const int pit, const bool flipBoard) {
  if (!flipBoard) {
    if (pit == mancala::kStore2) return fui::makeRect(28, 168, 46, 316);
    if (pit == mancala::kStore1) return fui::makeRect(406, 168, 46, 316);
    if (pit >= 0 && pit < 6) {
      const int16_t px = static_cast<int16_t>(82 + pit * 54);
      return fui::makeRect(px, 338, 46, 146);
    }
    if (pit >= 7 && pit < 13) {
      const int c = 12 - pit;
      const int16_t px = static_cast<int16_t>(82 + c * 54);
      return fui::makeRect(px, 168, 46, 146);
    }
  } else {
    // Flipped: North player's perspective (pits 7..12 at bottom, Store 2 on right)
    if (pit == mancala::kStore1) return fui::makeRect(28, 168, 46, 316);
    if (pit == mancala::kStore2) return fui::makeRect(406, 168, 46, 316);
    if (pit >= 7 && pit < 13) {
      const int16_t px = static_cast<int16_t>(82 + (pit - 7) * 54);
      return fui::makeRect(px, 338, 46, 146);
    }
    if (pit >= 0 && pit < 6) {
      const int c = 5 - pit;
      const int16_t px = static_cast<int16_t>(82 + c * 54);
      return fui::makeRect(px, 168, 46, 146);
    }
  }
  return fui::Rect{};
}

int pitAt(const int x, const int y, const bool flipBoard) {
  if (!flipBoard) {
    if (x >= 24 && x <= 76 && y >= 164 && y <= 488) return mancala::kStore2;
    if (x >= 402 && x <= 454 && y >= 164 && y <= 488) return mancala::kStore1;

    for (int c = 0; c < 6; ++c) {
      const int px = 82 + c * 54;
      if (x >= px && x < px + 46) {
        if (y >= 168 && y <= 316) return 12 - c;
        if (y >= 338 && y <= 486) return c;
      }
    }
  } else {
    if (x >= 24 && x <= 76 && y >= 164 && y <= 488) return mancala::kStore1;
    if (x >= 402 && x <= 454 && y >= 164 && y <= 488) return mancala::kStore2;

    for (int c = 0; c < 6; ++c) {
      const int px = 82 + c * 54;
      if (x >= px && x < px + 46) {
        if (y >= 168 && y <= 316) return 5 - c;
        if (y >= 338 && y <= 486) return 7 + c;
      }
    }
  }
  return -1;
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const char* modeLabel = (model.mode == PlayMode::Nearby)
                              ? "NEARBY LINK"
                              : ((model.mode == PlayMode::SoloVsAI) ? "SOLO AI" : "PASS & PLAY");
  toyboxChrome(screen, "MANCALA", modeLabel);

  // Score & Turn status row
  const fui::Rect scoreRow = screen.takeTop(34);

  // North store count (left)
  char northText[24];
  std::snprintf(northText, sizeof(northText), "NORTH: %d", model.game.pits[mancala::kStore2]);
  fui::TextStyle leftStyle;
  leftStyle.font = toybox::kTileFont;
  leftStyle.color = fui::Color::Black;
  leftStyle.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 8), scoreRow.y, 110, scoreRow.height),
                       northText, leftStyle);

  // South store count (right)
  char southText[24];
  std::snprintf(southText, sizeof(southText), "SOUTH: %d", model.game.pits[mancala::kStore1]);
  fui::TextStyle rightStyle;
  rightStyle.font = toybox::kTileFont;
  rightStyle.color = fui::Color::Black;
  rightStyle.align = fui::TextAlign::Right;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.right() - 118), scoreRow.y, 110, scoreRow.height),
                       southText, rightStyle);

  // Center turn / status message
  char statusBuf[48] = {};
  if (model.game.isOver) {
    const mancala::Outcome res = mancala::outcome(model.game);
    if (res == mancala::Outcome::Draw) {
      std::snprintf(statusBuf, sizeof(statusBuf), "GAME OVER: DRAW");
    } else if (model.mode == PlayMode::Nearby || model.mode == PlayMode::SoloVsAI) {
      const bool won = (res == mancala::Outcome::Player1Won && model.humanPlayer == mancala::kPlayer1) ||
                       (res == mancala::Outcome::Player2Won && model.humanPlayer == mancala::kPlayer2);
      std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                    won ? "YOU WIN!" : (model.mode == PlayMode::Nearby ? "OPPONENT WINS" : "AI WINS"));
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "%s WINS",
                    res == mancala::Outcome::Player1Won ? "SOUTH" : "NORTH");
    }
  } else if (model.game.extraTurn) {
    if (model.mode == PlayMode::Nearby) {
      const bool yourTurn = model.game.turn == model.humanPlayer;
      std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "EXTRA TURN! GO AGAIN" : "OPPONENT'S EXTRA TURN");
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "EXTRA TURN! GO AGAIN");
    }
  } else if (model.isAiThinking) {
    std::snprintf(statusBuf, sizeof(statusBuf), "AI THINKING...");
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
  } else if (model.mode == PlayMode::SoloVsAI) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN (SOUTH)" : "AI TURN (NORTH)");
  } else {
    std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                  model.game.turn == mancala::kPlayer1 ? "PASS TO SOUTH (P1)" : "PASS TO NORTH (P2)");
  }

  fui::TextStyle centerStyle;
  centerStyle.font = toybox::kTileFont;
  centerStyle.color = fui::Color::Black;
  centerStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 115), scoreRow.y,
                                     static_cast<int16_t>(scoreRow.width - 230), scoreRow.height),
                       statusBuf, centerStyle);

  // Hairline separator under score row
  screen.target().fill(fui::makeRect(scoreRow.x, static_cast<int16_t>(scoreRow.bottom() + 1),
                                     scoreRow.width, toybox::kHairline),
                       fui::Paint::solid(fui::Color::Black));

  // Mancala Board
  const fui::Rect board = boardRect();
  // Wooden board outer frame & surface
  screen.target().fill(board, fui::Paint::dither(fui::Color::LightGray));
  screen.target().stroke(fui::makeRect(static_cast<int16_t>(board.x - toybox::kFrame),
                                       static_cast<int16_t>(board.y - toybox::kFrame),
                                       static_cast<int16_t>(board.width + toybox::kFrame * 2),
                                       static_cast<int16_t>(board.height + toybox::kFrame * 2)),
                         fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  // Draw Stores
  const char* store2Label = (model.mode == PlayMode::SoloVsAI) ? "AI" : ((model.mode == PlayMode::Nearby) ? "THEM" : "N");
  const char* store1Label = (model.mode == PlayMode::SoloVsAI) ? "YOU" : ((model.mode == PlayMode::Nearby) ? "YOU" : "S");
  drawStore(screen, pitRect(mancala::kStore2, model.flipBoard), store2Label, model.game.pits[mancala::kStore2]);
  drawStore(screen, pitRect(mancala::kStore1, model.flipBoard), store1Label, model.game.pits[mancala::kStore1]);

  // Determine which pits should be highlighted as tappable
  const bool canMove = !model.game.isOver && !model.isAiThinking &&
                       ((model.mode == PlayMode::TwoPlayerPassAndPlay) || (model.game.turn == model.humanPlayer));

  // Draw North Pits (12..7, left to right)
  for (int pit = 12; pit >= 7; --pit) {
    const bool hl = canMove && model.game.turn == mancala::kPlayer2 && model.game.pits[pit] > 0;
    drawPitBowl(screen, pitRect(pit, model.flipBoard), model.game.pits[pit], hl);
  }

  // Draw South Pits (0..5, left to right)
  for (int pit = 0; pit < 6; ++pit) {
    const bool hl = canMove && model.game.turn == mancala::kPlayer1 && model.game.pits[pit] > 0;
    drawPitBowl(screen, pitRect(pit, model.flipBoard), model.game.pits[pit], hl);
  }

  // Bottom action buttons
  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "BACK TO LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);

  if (model.mode == PlayMode::TwoPlayerPassAndPlay) {
    const int16_t third = static_cast<int16_t>((actionRow.width - 2 * toybox::kGutter) / 3);
    fui::ButtonProps restartBtn;
    restartBtn.label = "NEW GAME";
    restartBtn.action = ActionRestart;
    restartBtn.text.font = toybox::kTileFont;
    restartBtn.text.align = fui::TextAlign::Center;
    screen.button(restartBtn, fui::makeRect(actionRow.x, actionRow.y, third, actionRow.height));

    fui::ButtonProps flipBtn;
    flipBtn.label = "FLIP";
    flipBtn.action = ActionFlipBoard;
    flipBtn.text.font = toybox::kTileFont;
    flipBtn.text.align = fui::TextAlign::Center;
    screen.button(flipBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + third + toybox::kGutter),
                                         actionRow.y, third, actionRow.height));

    fui::ButtonProps modeBtn;
    modeBtn.label = "MODE: 2P";
    modeBtn.action = ActionToggleMode;
    modeBtn.text.font = toybox::kTileFont;
    modeBtn.text.align = fui::TextAlign::Center;
    screen.button(modeBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + (third + toybox::kGutter) * 2),
                                        actionRow.y, third, actionRow.height));
  } else {
    const int16_t half = static_cast<int16_t>((actionRow.width - toybox::kGutter) / 2);
    fui::ButtonProps leftBtn;
    leftBtn.label = "NEW GAME";
    leftBtn.action = ActionRestart;
    screen.button(leftBtn, fui::makeRect(actionRow.x, actionRow.y, half, actionRow.height));

    fui::ButtonProps rightBtn;
    rightBtn.label = (model.mode == PlayMode::Nearby)
                         ? "MODE: NEARBY"
                         : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
    rightBtn.action = ActionToggleMode;
    screen.button(rightBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter),
                                          actionRow.y, half, actionRow.height));
  }

  // Hint / status text
  char hintBuf[64] = {};
  if (model.game.isOver) {
    const int s1 = model.game.pits[mancala::kStore1];
    const int s2 = model.game.pits[mancala::kStore2];
    std::snprintf(hintBuf, sizeof(hintBuf), "Final Score: South %d - North %d", s1, s2);
  } else if (model.game.extraTurn) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Last seed landed in store! Take another turn.");
  } else if (model.mode == PlayMode::Nearby && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Opponent is sowing seeds...");
  } else if (model.mode == PlayMode::Nearby) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Your turn! Tap a pit to sow seeds.");
  } else if (model.mode == PlayMode::SoloVsAI && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "AI calculating sowing strategy...");
  } else if (model.mode == PlayMode::TwoPlayerPassAndPlay) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Pass to %s (tap FLIP to rotate table)",
                  model.game.turn == mancala::kPlayer1 ? "South (P1)" : "North (P2)");
  } else {
    std::snprintf(hintBuf, sizeof(hintBuf), "Tap a highlighted pit to sow seeds counter-clockwise.");
  }

  fui::TextStyle hintStyle;
  hintStyle.font = toybox::kTileFont;
  hintStyle.color = fui::Color::Black;
  hintStyle.align = fui::TextAlign::Center;
  const int16_t hintY = static_cast<int16_t>(board.bottom() + 10);
  const int16_t hintH = static_cast<int16_t>(actionRow.y - board.bottom() - 14);
  if (hintH > 0) {
    screen.target().text(fui::makeRect(board.x, hintY, board.width, hintH), hintBuf, hintStyle);
  }
}

}  // namespace mancalaui
