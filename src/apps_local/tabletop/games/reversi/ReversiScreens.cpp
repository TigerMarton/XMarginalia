#include "ReversiScreens.h"

#include <cstdio>
#include <cstring>

#include "../../../ui/ToyboxFormat.h"

namespace reversiui {

namespace {

constexpr int16_t kBoardLeft = 16;
constexpr int16_t kBoardTop = 144;
constexpr int16_t kCellSize = 56;
constexpr int16_t kBoardSide = kCellSize * reversi::kSize;  // 448

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

}  // namespace

fui::Rect boardRect() {
  return fui::makeRect(kBoardLeft, kBoardTop, kBoardSide, kBoardSide);
}

fui::Rect cellRect(const int col, const int row) {
  return fui::makeRect(static_cast<int16_t>(kBoardLeft + col * kCellSize),
                       static_cast<int16_t>(kBoardTop + row * kCellSize),
                       kCellSize, kCellSize);
}

int cellAt(const int x, const int y, int& outCol, int& outRow, const bool flipBoard) {
  const int dx = x - kBoardLeft;
  const int dy = y - kBoardTop;
  if (dx < 0 || dy < 0 || dx >= kBoardSide || dy >= kBoardSide) return -1;
  int c = dx / kCellSize;
  int r = dy / kCellSize;
  if (c >= reversi::kSize) c = reversi::kSize - 1;
  if (r >= reversi::kSize) r = reversi::kSize - 1;
  outCol = flipBoard ? (reversi::kSize - 1 - c) : c;
  outRow = flipBoard ? (reversi::kSize - 1 - r) : r;
  return outRow * reversi::kSize + outCol;
}

void buildBoard(toybox::Screen& screen, const BoardModel& model) {
  const char* modeLabel = (model.mode == PlayMode::Nearby)
                              ? "NEARBY LINK"
                              : ((model.mode == PlayMode::SoloVsAI) ? "SOLO AI" : "PASS & PLAY");
  toyboxChrome(screen, "REVERSI", modeLabel);

  // Score & Turn status row
  const fui::Rect scoreRow = screen.takeTop(34);

  // Black score (left)
  const int16_t blackDiscX = static_cast<int16_t>(scoreRow.x + 12);
  const int16_t discY = static_cast<int16_t>(scoreRow.y + scoreRow.height / 2);
  toybox::disc(screen, blackDiscX, discY, 9, fui::Color::Black);
  char blackText[16];
  std::snprintf(blackText, sizeof(blackText), "%d", reversi::blackCount(model.game));
  fui::TextStyle scoreStyle;
  scoreStyle.font = toybox::kUiFont;
  scoreStyle.color = fui::Color::Black;
  scoreStyle.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 28), scoreRow.y, 48, scoreRow.height),
                       blackText, scoreStyle);

  // White score (right)
  const int16_t whiteDiscX = static_cast<int16_t>(scoreRow.right() - 50);
  toybox::disc(screen, whiteDiscX, discY, 9, fui::Color::Black);
  toybox::disc(screen, whiteDiscX, discY, 7, fui::Color::White);
  char whiteText[16];
  std::snprintf(whiteText, sizeof(whiteText), "%d", reversi::whiteCount(model.game));
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.right() - 36), scoreRow.y, 36, scoreRow.height),
                       whiteText, scoreStyle);

  // Center turn indicator / status
  char statusBuf[48] = {};
  if (model.game.isOver) {
    const reversi::Outcome res = reversi::outcome(model.game);
    if (res == reversi::Outcome::Draw) {
      std::snprintf(statusBuf, sizeof(statusBuf), "DRAW");
    } else if (model.mode == PlayMode::Nearby || model.mode == PlayMode::SoloVsAI) {
      const bool won = (res == reversi::Outcome::BlackWon && model.humanSeat == reversi::kBlack) ||
                       (res == reversi::Outcome::WhiteWon && model.humanSeat == reversi::kWhite);
      std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                    won ? "YOU WIN!" : (model.mode == PlayMode::Nearby ? "OPPONENT WINS" : "AI WINS"));
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "%s WINS",
                    res == reversi::Outcome::BlackWon ? "BLACK" : "WHITE");
    }
  } else if (model.isAiThinking) {
    std::snprintf(statusBuf, sizeof(statusBuf), "AI THINKING...");
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = model.game.turn == model.humanSeat;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
  } else if (model.mode == PlayMode::SoloVsAI) {
    const bool yourTurn = model.game.turn == model.humanSeat;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN (BLACK)" : "AI TURN (WHITE)");
  } else {
    std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                  model.game.turn == reversi::kBlack ? "PASS TO BLACK" : "PASS TO WHITE");
  }

  fui::TextStyle centerStyle;
  centerStyle.font = toybox::kTileFont;
  centerStyle.color = fui::Color::Black;
  centerStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 80), scoreRow.y,
                                     static_cast<int16_t>(scoreRow.width - 160), scoreRow.height),
                       statusBuf, centerStyle);

  // Hairline rule under score row
  screen.target().fill(fui::makeRect(scoreRow.x, static_cast<int16_t>(scoreRow.bottom() + 2),
                                     scoreRow.width, toybox::kHairline),
                       fui::Paint::solid(fui::Color::Black));

  // Board Drawing
  const fui::Rect board = boardRect();
  screen.target().stroke(fui::makeRect(static_cast<int16_t>(board.x - toybox::kFrame),
                                       static_cast<int16_t>(board.y - toybox::kFrame),
                                       static_cast<int16_t>(board.width + toybox::kFrame * 2),
                                       static_cast<int16_t>(board.height + toybox::kFrame * 2)),
                         fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  for (int r = 0; r < reversi::kSize; ++r) {
    for (int c = 0; c < reversi::kSize; ++c) {
      const int drawC = model.flipBoard ? (reversi::kSize - 1 - c) : c;
      const int drawR = model.flipBoard ? (reversi::kSize - 1 - r) : r;
      const fui::Rect cell = cellRect(drawC, drawR);
      screen.target().fill(cell, fui::Paint::dither(fui::Color::LightGray));
      screen.target().stroke(cell, fui::Paint::solid(fui::Color::Black), toybox::kHairline);

      const uint8_t piece = reversi::cellAt(model.game, c, r);
      const int16_t cx = static_cast<int16_t>(cell.x + cell.width / 2);
      const int16_t cy = static_cast<int16_t>(cell.y + cell.height / 2);

      if (piece == reversi::kBlack) {
        toybox::disc(screen, cx, cy, 22, fui::Color::Black);
      } else if (piece == reversi::kWhite) {
        toybox::disc(screen, cx, cy, 22, fui::Color::Black);
        toybox::disc(screen, cx, cy, 19, fui::Color::White);
      } else {
        const uint64_t mask = 1ULL << (r * reversi::kSize + c);
        if (model.legalMoves & mask) {
          toybox::disc(screen, cx, cy, 5, fui::Color::Black);
        }
      }
    }
  }

  // 4 guide dots on standard Othello intersections: (2,2), (6,2), (2,6), (6,6)
  toybox::disc(screen, static_cast<int16_t>(board.x + 2 * kCellSize),
               static_cast<int16_t>(board.y + 2 * kCellSize), 3, fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(board.x + 6 * kCellSize),
               static_cast<int16_t>(board.y + 2 * kCellSize), 3, fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(board.x + 2 * kCellSize),
               static_cast<int16_t>(board.y + 6 * kCellSize), 3, fui::Color::Black);
  toybox::disc(screen, static_cast<int16_t>(board.x + 6 * kCellSize),
               static_cast<int16_t>(board.y + 6 * kCellSize), 3, fui::Color::Black);

  // Bottom Buttons
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
    if (!model.game.isOver && model.legalMoves == 0 &&
        reversi::hasLegalMoves(model.game, reversi::opponent(model.game.turn))) {
      rightBtn.label = "PASS TURN";
      rightBtn.action = ActionPass;
    } else {
      rightBtn.label = (model.mode == PlayMode::Nearby)
                            ? "MODE: NEARBY"
                            : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
      rightBtn.action = ActionToggleMode;
    }
    screen.button(rightBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter),
                                          actionRow.y, half, actionRow.height));
  }

  // Hint banner
  char hintBuf[64] = {};
  if (model.game.isOver) {
    const int b = reversi::blackCount(model.game);
    const int w = reversi::whiteCount(model.game);
    std::snprintf(hintBuf, sizeof(hintBuf), "Final Score: Black %d - White %d", b, w);
  } else if (model.legalMoves == 0) {
    std::snprintf(hintBuf, sizeof(hintBuf), "No moves available. Tap PASS TURN.");
  } else if (model.mode == PlayMode::Nearby && model.game.turn != model.humanSeat) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Opponent's turn (waiting for move...)");
  } else if (model.mode == PlayMode::Nearby) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Your turn! Tap a square to play.");
  } else if (model.mode == PlayMode::SoloVsAI && model.game.turn != model.humanSeat) {
    std::snprintf(hintBuf, sizeof(hintBuf), "AI calculating next move...");
  } else if (model.mode == PlayMode::TwoPlayerPassAndPlay) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Pass to %s (tap FLIP to rotate table)",
                  model.game.turn == reversi::kBlack ? "Black" : "White");
  } else {
    std::snprintf(hintBuf, sizeof(hintBuf), "Tap a dotted square to place disc.");
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

}  // namespace reversiui
