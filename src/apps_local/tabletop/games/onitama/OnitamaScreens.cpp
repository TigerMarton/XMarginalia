#include "OnitamaScreens.h"

#include <cstdio>
#include <cstring>

namespace onitamaui {
namespace fui = freeink::ui;
namespace {

constexpr int16_t kBoardLeft = 65;
constexpr int16_t kBoardTop = 175;
constexpr int16_t kCellSize = 70;
constexpr int16_t kBoardWidth = 350;
constexpr int16_t kBoardHeight = 350;

fui::Rect cellRect(const int index, const bool flipBoard) {
  int x = index % onitama::kBoardWidth;
  int y = index / onitama::kBoardWidth;
  if (flipBoard) {
    x = 4 - x;
    y = 4 - y;
  }
  return fui::makeRect(static_cast<int16_t>(kBoardLeft + x * kCellSize),
                       static_cast<int16_t>(kBoardTop + y * kCellSize),
                       kCellSize, kCellSize);
}

void drawToriiArch(toybox::Screen& screen, const fui::Rect& box) {
  // Temple arch outline on temple squares (2,0) and (2,4)
  const int16_t midX = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t topY = static_cast<int16_t>(box.y + 8);

  // Top lintel
  screen.target().fill(fui::makeRect(static_cast<int16_t>(box.x + 10), topY, static_cast<int16_t>(box.width - 20), 4),
                       fui::Paint::solid(fui::Color::Black));
  // Secondary crossbeam
  screen.target().fill(fui::makeRect(static_cast<int16_t>(box.x + 14), static_cast<int16_t>(topY + 7), static_cast<int16_t>(box.width - 28), 3),
                       fui::Paint::solid(fui::Color::Black));
  // Left and right pillars
  screen.target().fill(fui::makeRect(static_cast<int16_t>(midX - 18), static_cast<int16_t>(topY + 7), 4, static_cast<int16_t>(box.height - 22)),
                       fui::Paint::solid(fui::Color::Black));
  screen.target().fill(fui::makeRect(static_cast<int16_t>(midX + 14), static_cast<int16_t>(topY + 7), 4, static_cast<int16_t>(box.height - 22)),
                       fui::Paint::solid(fui::Color::Black));
}

void drawPiece(toybox::Screen& screen, const fui::Rect& box, const uint8_t piece) {
  if (piece == onitama::kEmpty) return;

  const int16_t midX = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t midY = static_cast<int16_t>(box.y + box.height / 2);

  const bool isP1Piece = onitama::isP1(piece);
  const bool isMasterPiece = onitama::isMaster(piece);
  const int16_t r = isMasterPiece ? 27 : 20;

  const fui::Rect pBox = fui::makeRect(static_cast<int16_t>(midX - r), static_cast<int16_t>(midY - r),
                                       static_cast<int16_t>(r * 2), static_cast<int16_t>(r * 2));

  if (isP1Piece) {
    // Player 1 (South / Red): White disc with solid black rim
    screen.target().fill(pBox, fui::Paint::solid(fui::Color::White));
    screen.target().stroke(pBox, fui::Paint::solid(fui::Color::Black), 3);
    if (isMasterPiece) {
      screen.target().stroke(fui::makeRect(static_cast<int16_t>(pBox.x + 4), static_cast<int16_t>(pBox.y + 4),
                                           static_cast<int16_t>(pBox.width - 8), static_cast<int16_t>(pBox.height - 8)),
                             fui::Paint::solid(fui::Color::Black), 2);
      fui::TextStyle masterStyle;
      masterStyle.font = toybox::kTileFont;
      masterStyle.color = fui::Color::Black;
      masterStyle.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(pBox.x, static_cast<int16_t>(midY - 10), pBox.width, 20), "M", masterStyle);
    }
  } else {
    // Player 2 (North / Blue): Solid black disc
    screen.target().fill(pBox, fui::Paint::solid(fui::Color::Black));
    if (isMasterPiece) {
      screen.target().stroke(pBox, fui::Paint::solid(fui::Color::Black), 3);
      screen.target().stroke(fui::makeRect(static_cast<int16_t>(pBox.x + 4), static_cast<int16_t>(pBox.y + 4),
                                           static_cast<int16_t>(pBox.width - 8), static_cast<int16_t>(pBox.height - 8)),
                             fui::Paint::solid(fui::Color::White), 2);
      fui::TextStyle masterStyle;
      masterStyle.font = toybox::kTileFont;
      masterStyle.color = fui::Color::White;
      masterStyle.align = fui::TextAlign::Center;
      screen.target().text(fui::makeRect(pBox.x, static_cast<int16_t>(midY - 10), pBox.width, 20), "M", masterStyle);
    }
  }
}

void drawMiniCard(toybox::Screen& screen, const fui::Rect& box, const uint8_t cardId, const bool isSelected,
                  const bool isTableCard, const uint8_t activePlayer) {
  const onitama::CardDef& card = onitama::kCards[cardId % 16];

  // Outer frame
  screen.target().fill(box, fui::Paint::solid(fui::Color::White));
  const int16_t strokeW = isSelected ? 4 : (isTableCard ? 1 : 2);
  screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), strokeW);

  // Card header text
  fui::TextStyle nameStyle;
  nameStyle.font = toybox::kTileFont;
  nameStyle.color = fui::Color::Black;
  nameStyle.align = fui::TextAlign::Center;

  char labelBuf[32];
  if (isTableCard) {
    std::snprintf(labelBuf, sizeof(labelBuf), "%s [NEXT]", card.name);
  } else {
    std::snprintf(labelBuf, sizeof(labelBuf), "%s", card.name);
  }
  screen.target().text(fui::makeRect(box.x, static_cast<int16_t>(box.y + 4), box.width, 18), labelBuf, nameStyle);

  // 5x5 Mini Movement Grid
  constexpr int16_t kDotStep = 10;
  const int16_t gridCenterX = static_cast<int16_t>(box.x + box.width / 2);
  const int16_t gridCenterY = static_cast<int16_t>(box.y + 54);

  // Draw 5x5 grid dots
  for (int dy = -2; dy <= 2; ++dy) {
    for (int dx = -2; dx <= 2; ++dx) {
      const int16_t px = static_cast<int16_t>(gridCenterX + dx * kDotStep);
      const int16_t py = static_cast<int16_t>(gridCenterY - dy * kDotStep);

      if (dx == 0 && dy == 0) {
        // Origin piece (center square)
        screen.target().fill(fui::makeRect(static_cast<int16_t>(px - 3), static_cast<int16_t>(py - 3), 7, 7),
                             fui::Paint::solid(fui::Color::Black));
      } else {
        // Light grid reference dot
        screen.target().fill(fui::makeRect(static_cast<int16_t>(px - 1), static_cast<int16_t>(py - 1), 3, 3),
                             fui::Paint::dither(fui::Color::DarkGray));
      }
    }
  }

  // Draw relative movement markers
  for (int m = 0; m < card.moveCount; ++m) {
    int dx = card.moves[m][0];
    int dy = card.moves[m][1];
    if (activePlayer == onitama::kPlayer2 && !isTableCard) {
      dx = -dx;
      dy = -dy;
    }

    const int16_t px = static_cast<int16_t>(gridCenterX + dx * kDotStep);
    const int16_t py = static_cast<int16_t>(gridCenterY - dy * kDotStep);

    // Large filled movement dot
    screen.target().fill(fui::makeRect(static_cast<int16_t>(px - 4), static_cast<int16_t>(py - 4), 9, 9),
                         fui::Paint::solid(fui::Color::Black));
  }
}

}  // namespace

int cellAt(const int x, const int y, const bool flipBoard) {
  if (x < kBoardLeft || x >= kBoardLeft + kBoardWidth || y < kBoardTop || y >= kBoardTop + kBoardHeight) {
    return -1;
  }
  int col = (x - kBoardLeft) / kCellSize;
  int row = (y - kBoardTop) / kCellSize;
  if (col < 0 || col >= 5 || row < 0 || row >= 5) return -1;

  if (flipBoard) {
    col = 4 - col;
    row = 4 - row;
  }
  return row * onitama::kBoardWidth + col;
}

int cardAt(const int x, const int y) {
  // Bottom cards tray: y in 535..630
  if (y >= 535 && y <= 630) {
    if (x >= 24 && x <= 164) return 0;
    if (x >= 170 && x <= 310) return 1;
  }
  return -1;
}

void buildBoard(toybox::Screen& screen, const ScreenModel& model) {
  // Top Header Banner
  fui::HeaderProps header;
  header.title = "ONITAMA";
  header.rightLabel = (model.mode == PlayMode::Nearby)
                          ? "NEARBY LINK"
                          : ((model.mode == PlayMode::TwoPlayerPassAndPlay) ? "PASS & PLAY" : "WAY OF THE WARRIOR");
  header.titleText.font = toybox::kDisplayFont;
  header.titleText.color = fui::Color::White;
  header.titleText.align = fui::TextAlign::Left;
  header.subtitleText.font = toybox::kTileFont;
  header.subtitleText.color = fui::Color::White;
  header.subtitleText.align = fui::TextAlign::Right;
  header.borderEdges = fui::EdgesNone;
  toybox::absoluteChrome(screen);
  toybox::headerBand(screen, header);
  screen.insetContent(fui::Insets{toybox::kGutter * 3, toybox::kMargin, toybox::kMargin, toybox::kMargin});

  // Score & Status Row
  const fui::Rect scoreRow = screen.takeTop(28);

  char northText[32];
  char southText[32];
  if (model.mode == PlayMode::SoloVsAI) {
    std::snprintf(northText, sizeof(northText), "AI (NORTH)");
    std::snprintf(southText, sizeof(southText), "YOU (SOUTH)");
  } else if (model.mode == PlayMode::Nearby) {
    std::snprintf(northText, sizeof(northText), "OPPONENT (NORTH)");
    std::snprintf(southText, sizeof(southText), "YOU (SOUTH)");
  } else {
    std::snprintf(northText, sizeof(northText), "P2 (NORTH)");
    std::snprintf(southText, sizeof(southText), "P1 (SOUTH)");
  }

  fui::TextStyle leftStyle;
  leftStyle.font = toybox::kTileFont;
  leftStyle.color = fui::Color::Black;
  leftStyle.align = fui::TextAlign::Left;
  screen.target().text(fui::makeRect(scoreRow.x, scoreRow.y, 110, scoreRow.height), northText, leftStyle);

  fui::TextStyle rightStyle;
  rightStyle.font = toybox::kTileFont;
  rightStyle.color = fui::Color::Black;
  rightStyle.align = fui::TextAlign::Right;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.right() - 110), scoreRow.y, 110, scoreRow.height),
                       southText, rightStyle);

  // Turn / Winner message
  char statusBuf[64];
  if (model.game.isOver) {
    if (model.mode == PlayMode::Nearby) {
      const bool won = model.game.winner == model.humanPlayer;
      const char* reason = (model.game.winReason == onitama::WinReason::Stone) ? "STONE" : "STREAM";
      std::snprintf(statusBuf, sizeof(statusBuf), "%s VIA %s", won ? "YOU WIN" : "OPPONENT WINS", reason);
    } else {
      const char* winName = (model.game.winner == onitama::kPlayer1) ? "SOUTH" : "NORTH";
      const char* reason = (model.game.winReason == onitama::WinReason::Stone) ? "STONE (MASTER CAPTURE)" : "STREAM (TEMPLE TAKEN)";
      std::snprintf(statusBuf, sizeof(statusBuf), "%s WINS VIA %s", winName, reason);
    }
  } else if (model.isAiThinking) {
    std::snprintf(statusBuf, sizeof(statusBuf), "AI CALCULATING CARD MOVE...");
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = model.game.turn == model.humanPlayer;
    std::snprintf(statusBuf, sizeof(statusBuf), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
  } else if (model.mode == PlayMode::SoloVsAI) {
    std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                  model.game.turn == model.humanPlayer ? "YOUR TURN (SOUTH)" : "AI TURN (NORTH)");
  } else {
    std::snprintf(statusBuf, sizeof(statusBuf), "%s",
                  model.game.turn == onitama::kPlayer1 ? "PASS TO SOUTH (P1)" : "PASS TO NORTH (P2)");
  }

  fui::TextStyle centerStyle;
  centerStyle.font = toybox::kTileFont;
  centerStyle.color = fui::Color::Black;
  centerStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(static_cast<int16_t>(scoreRow.x + 112), scoreRow.y,
                                     static_cast<int16_t>(scoreRow.width - 224), scoreRow.height),
                       statusBuf, centerStyle);

  // Hairline under status row
  screen.target().fill(fui::makeRect(scoreRow.x, static_cast<int16_t>(scoreRow.bottom() + 2), scoreRow.width, toybox::kHairline),
                       fui::Paint::solid(fui::Color::Black));

  // Compute legal destination moves for the selected piece & card
  uint32_t destMask = 0;
  if (!model.game.isOver && model.selectedCell >= 0 && model.selectedCard >= 0) {
    onitama::Move moves[64];
    const int count = onitama::legalMoves(model.game, moves, 64);
    for (int i = 0; i < count; ++i) {
      if (moves[i].from == static_cast<uint8_t>(model.selectedCell) &&
          moves[i].cardIdx == static_cast<uint8_t>(model.selectedCard)) {
        destMask |= (1u << moves[i].to);
      }
    }
  }

  // 5x5 Board
  const fui::Rect boardBox = fui::makeRect(kBoardLeft, kBoardTop, kBoardWidth, kBoardHeight);
  screen.target().fill(boardBox, fui::Paint::solid(fui::Color::White));
  screen.target().stroke(fui::makeRect(static_cast<int16_t>(boardBox.x - toybox::kFrame),
                                       static_cast<int16_t>(boardBox.y - toybox::kFrame),
                                       static_cast<int16_t>(boardBox.width + toybox::kFrame * 2),
                                       static_cast<int16_t>(boardBox.height + toybox::kFrame * 2)),
                         fui::Paint::solid(fui::Color::Black), toybox::kFrame);

  // Cells
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 5; ++col) {
      const int idx = row * 5 + col;
      const fui::Rect box = cellRect(idx, model.flipBoard);

      // Checkered surface
      if ((row + col) % 2 == 1) {
        screen.target().fill(box, fui::Paint::dither(fui::Color::LightGray));
      }

      // Temple arches on (2,0) and (2,4)
      if (idx == onitama::kP2Temple || idx == onitama::kP1Temple) {
        drawToriiArch(screen, box);
      }

      // Cell border lines
      screen.target().stroke(box, fui::Paint::solid(fui::Color::Black), 1);

      // Selected cell bracket
      if (idx == model.selectedCell) {
        toybox::bracket(screen, box, 10, 3);
      }

      // Legal destination highlight
      if ((destMask & (1u << idx)) != 0) {
        if (model.game.cells[idx] != onitama::kEmpty) {
          // Capturing enemy piece: draw bracket around target
          toybox::bracket(screen, box, 10, 3);
        } else {
          // Vacant cell: solid black center target dot
          const int16_t cx = static_cast<int16_t>(box.x + box.width / 2);
          const int16_t cy = static_cast<int16_t>(box.y + box.height / 2);
          screen.target().fill(fui::makeRect(static_cast<int16_t>(cx - 4), static_cast<int16_t>(cy - 4), 9, 9),
                               fui::Paint::solid(fui::Color::Black));
        }
      }

      // Draw Piece
      drawPiece(screen, box, model.game.cells[idx]);
    }
  }

  // Active Player Cards Tray (Bottom)
  const uint8_t active = model.game.turn;
  const uint8_t* myCards = (active == onitama::kPlayer1) ? model.game.p1Cards : model.game.p2Cards;

  constexpr int16_t kCardW = 140;
  constexpr int16_t kCardH = 92;
  constexpr int16_t kCardY = 535;

  // Card 0
  drawMiniCard(screen, fui::makeRect(24, kCardY, kCardW, kCardH), myCards[0], model.selectedCard == 0, false, active);
  // Card 1
  drawMiniCard(screen, fui::makeRect(170, kCardY, kCardW, kCardH), myCards[1], model.selectedCard == 1, false, active);
  // Next Table Card
  drawMiniCard(screen, fui::makeRect(316, kCardY, kCardW, kCardH), model.game.tableCard, false, true, active);

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
    screen.button(flipBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + third + toybox::kGutter), actionRow.y, third, actionRow.height));

    fui::ButtonProps modeBtn;
    modeBtn.label = "MODE: 2P";
    modeBtn.action = ActionToggleMode;
    modeBtn.text.font = toybox::kTileFont;
    modeBtn.text.align = fui::TextAlign::Center;
    screen.button(modeBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + (third + toybox::kGutter) * 2), actionRow.y, third, actionRow.height));
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
    screen.button(rightBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter), actionRow.y, half, actionRow.height));
  }

  // Hint / Instruction Line
  char hintBuf[64];
  if (model.game.isOver) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Game Over! Tap NEW GAME to play again.");
  } else if (model.mode == PlayMode::Nearby && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Opponent's turn (waiting for move...)");
  } else if (model.mode == PlayMode::SoloVsAI && model.game.turn != model.humanPlayer) {
    std::snprintf(hintBuf, sizeof(hintBuf), "AI calculating martial arts technique...");
  } else if (model.selectedCard < 0) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Tap a card below to choose your movement technique.");
  } else if (model.selectedCell < 0) {
    std::snprintf(hintBuf, sizeof(hintBuf), "Tap your piece to see legal movement destinations.");
  } else {
    std::snprintf(hintBuf, sizeof(hintBuf), "Tap a highlighted square to execute your move.");
  }

  fui::TextStyle hintStyle;
  hintStyle.font = toybox::kTileFont;
  hintStyle.color = fui::Color::Black;
  hintStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(kBoardLeft, static_cast<int16_t>(actionRow.y - 30), kBoardWidth, 24), hintBuf, hintStyle);
}

}  // namespace onitamaui
