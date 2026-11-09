#include "DobutsuScreens.h"
#include <cstdio>

namespace dobutsuui {

namespace fui = freeink::ui;

namespace {

constexpr int16_t kTileW = 100;
constexpr int16_t kTileH = 92;
constexpr int16_t kBoardW = kTileW * dobutsu::kCols;  // 300
constexpr int16_t kBoardH = kTileH * dobutsu::kRows;  // 368
constexpr int16_t kDotSize = 6;

int8_t orientedSq(int8_t sq, bool flip) {
  if (!flip) return sq;
  const int8_t col = static_cast<int8_t>(sq % dobutsu::kCols);
  const int8_t row = static_cast<int8_t>(sq / dobutsu::kCols);
  return static_cast<int8_t>((dobutsu::kRows - 1 - row) * dobutsu::kCols + (dobutsu::kCols - 1 - col));
}

void drawMoveDot(toybox::Screen& screen, const fui::Rect& box, int dx, int dy) {
  // dx in {-1, 0, 1}, dy in {-1, 0, 1}
  int16_t px = static_cast<int16_t>(box.x + box.width / 2 - kDotSize / 2);
  if (dx < 0) px = static_cast<int16_t>(box.x + 8);
  else if (dx > 0) px = static_cast<int16_t>(box.x + box.width - 8 - kDotSize);

  int16_t py = static_cast<int16_t>(box.y + box.height / 2 - kDotSize / 2);
  if (dy < 0) py = static_cast<int16_t>(box.y + 8);
  else if (dy > 0) py = static_cast<int16_t>(box.y + box.height - 8 - kDotSize);

  screen.target().fill(fui::makeRect(px, py, kDotSize, kDotSize), fui::Paint::solid(fui::Color::Black));
}

void drawPiece(toybox::Screen& screen, const fui::Rect& cellBox, dobutsu::Cell piece, bool isSelected, bool flip) {
  if (piece == 0) return;

  const dobutsu::Player owner = dobutsu::pieceOwner(piece);
  const dobutsu::PieceType type = dobutsu::pieceType(piece);

  // Piece tile inside cell with inset
  const fui::Rect pBox = fui::makeRect(static_cast<int16_t>(cellBox.x + 6),
                                       static_cast<int16_t>(cellBox.y + 6),
                                       static_cast<int16_t>(cellBox.width - 12),
                                       static_cast<int16_t>(cellBox.height - 12));

  // Background and border
  screen.target().fill(pBox, fui::Paint::solid(fui::Color::White));
  const int16_t strokeW = isSelected ? 4 : 2;
  screen.target().stroke(pBox, fui::Paint::solid(fui::Color::Black), strokeW);

  // Owner directional indicator: P1 points up (unless flipped), P2 points down
  const bool pointsUp = (owner == dobutsu::Player::P1) ? !flip : flip;
  const int16_t chevY = pointsUp ? static_cast<int16_t>(pBox.y + 4)
                                 : static_cast<int16_t>(pBox.y + pBox.height - 8);
  screen.target().fill(fui::makeRect(static_cast<int16_t>(pBox.x + pBox.width / 2 - 8), chevY, 16, 3),
                       fui::Paint::solid(fui::Color::Black));

  // Determine movement dot direction relative to visual screen
  // If pointsUp is true: forward is dy = -1. If false: forward is dy = +1.
  const int fwdY = pointsUp ? -1 : 1;

  switch (type) {
    case dobutsu::PieceType::Lion:
      // 8 directions
      drawMoveDot(screen, pBox, 0, -1);
      drawMoveDot(screen, pBox, 0, 1);
      drawMoveDot(screen, pBox, -1, 0);
      drawMoveDot(screen, pBox, 1, 0);
      drawMoveDot(screen, pBox, -1, -1);
      drawMoveDot(screen, pBox, 1, -1);
      drawMoveDot(screen, pBox, -1, 1);
      drawMoveDot(screen, pBox, 1, 1);
      break;
    case dobutsu::PieceType::Giraffe:
      // 4 orthogonal
      drawMoveDot(screen, pBox, 0, -1);
      drawMoveDot(screen, pBox, 0, 1);
      drawMoveDot(screen, pBox, -1, 0);
      drawMoveDot(screen, pBox, 1, 0);
      break;
    case dobutsu::PieceType::Elephant:
      // 4 diagonal
      drawMoveDot(screen, pBox, -1, -1);
      drawMoveDot(screen, pBox, 1, -1);
      drawMoveDot(screen, pBox, -1, 1);
      drawMoveDot(screen, pBox, 1, 1);
      break;
    case dobutsu::PieceType::Chick:
      // 1 forward
      drawMoveDot(screen, pBox, 0, fwdY);
      break;
    case dobutsu::PieceType::Hen:
      // 4 orthogonal + 2 forward diagonals
      drawMoveDot(screen, pBox, 0, -1);
      drawMoveDot(screen, pBox, 0, 1);
      drawMoveDot(screen, pBox, -1, 0);
      drawMoveDot(screen, pBox, 1, 0);
      drawMoveDot(screen, pBox, -1, fwdY);
      drawMoveDot(screen, pBox, 1, fwdY);
      break;
    default:
      break;
  }

  // Animal Label / Initials: large cut 30 initial
  const char* label = "?";
  switch (type) {
    case dobutsu::PieceType::Lion: label = "L"; break;
    case dobutsu::PieceType::Giraffe: label = "G"; break;
    case dobutsu::PieceType::Elephant: label = "E"; break;
    case dobutsu::PieceType::Chick: label = "C"; break;
    case dobutsu::PieceType::Hen: label = "H"; break;
    default: break;
  }

  fui::TextStyle style;
  style.font = toybox::kDisplayFont;
  style.color = fui::Color::Black;
  style.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(pBox.x, static_cast<int16_t>(pBox.y + pBox.height / 2 - 15), pBox.width, 30),
                       label, style);
}

}  // namespace

int cellAt(const int x, const int y, const bool flip) {
  const fui::Rect grid = boardGridRect(fui::makeRect(16, 24, 448, 700));
  if (x < grid.x || x >= grid.x + grid.width || y < grid.y || y >= grid.y + grid.height) {
    return -1;
  }
  int col = (x - grid.x) / kTileW;
  int row = (y - grid.y) / kTileH;
  if (col < 0 || col >= dobutsu::kCols || row < 0 || row >= dobutsu::kRows) {
    return -1;
  }
  if (flip) {
    col = dobutsu::kCols - 1 - col;
    row = dobutsu::kRows - 1 - row;
  }
  return row * dobutsu::kCols + col;
}

fui::Rect boardGridRect(const fui::Rect& content) {
  const int16_t bx = static_cast<int16_t>(content.x + (content.width - kBoardW) / 2);
  const int16_t by = static_cast<int16_t>(content.y + 56);
  return fui::makeRect(bx, by, kBoardW, kBoardH);
}

fui::Rect cellRect(const fui::Rect& grid, int8_t sq, bool flip) {
  const int8_t renderSq = orientedSq(sq, flip);
  const int8_t col = static_cast<int8_t>(renderSq % dobutsu::kCols);
  const int8_t row = static_cast<int8_t>(renderSq / dobutsu::kCols);
  return fui::makeRect(static_cast<int16_t>(grid.x + col * kTileW),
                       static_cast<int16_t>(grid.y + row * kTileH),
                       kTileW, kTileH);
}

void buildBoard(toybox::Screen& screen, const ScreenModel& model) {
  const auto& game = model.game;

  // Header band
  fui::HeaderProps header;
  header.title = "DOBUTSU SHOGI";
  header.rightLabel = (model.mode == PlayMode::Nearby)
                          ? "NEARBY LINK"
                          : ((model.mode == PlayMode::SoloVsAI) ? "CATCH THE LION" : "PASS & PLAY");
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

  // Bottom Navigation Buttons
  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "BACK TO LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, screen.takeBottom(toybox::kPillHeight, toybox::kGutter));

  const fui::Rect actionRow = screen.takeBottom(toybox::kPillHeight, toybox::kGutter);

  if (game.isOver) {
    fui::ButtonProps playAgainBtn;
    playAgainBtn.label = "PLAY AGAIN";
    playAgainBtn.action = ActionRestart;
    screen.button(playAgainBtn, actionRow);
  } else {
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
      fui::ButtonProps restartBtn;
      restartBtn.label = "NEW GAME";
      restartBtn.action = ActionRestart;
      screen.button(restartBtn, fui::makeRect(actionRow.x, actionRow.y, half, actionRow.height));

      fui::ButtonProps modeBtn;
      modeBtn.label = (model.mode == PlayMode::Nearby)
                          ? "MODE: NEARBY"
                          : ((model.mode == PlayMode::SoloVsAI) ? "MODE: SOLO" : "MODE: 2P");
      modeBtn.action = ActionToggleMode;
      screen.button(modeBtn, fui::makeRect(static_cast<int16_t>(actionRow.x + half + toybox::kGutter), actionRow.y, half, actionRow.height));
    }
  }

  // Pre-calculate Legal Moves for selection
  dobutsu::Move legalMoves[48];
  size_t nLegal = 0;
  if (!game.isOver && !model.isAiThinking) {
    nLegal = dobutsu::generateLegalMoves(game, legalMoves, 48);
  }

  auto isLegalDest = [&](int8_t toSq) -> bool {
    if (model.selectedSq == -1) return false;
    for (size_t i = 0; i < nLegal; ++i) {
      if (legalMoves[i].from == model.selectedSq && legalMoves[i].to == toSq) {
        return true;
      }
    }
    return false;
  };

  const fui::Rect content = screen.contentRect();

  // Top Area: North / P2 Reserve Tray
  const dobutsu::Player northPlayer = model.flipBoard ? dobutsu::Player::P1 : dobutsu::Player::P2;
  const int northIdx = (northPlayer == dobutsu::Player::P1) ? 0 : 1;

  char northLabel[64];
  if (model.mode == PlayMode::SoloVsAI && !model.flipBoard) {
    std::snprintf(northLabel, sizeof(northLabel), "NORTH (AI) RESERVE: G:%d  E:%d  C:%d",
                  game.reserves[northIdx][0], game.reserves[northIdx][1], game.reserves[northIdx][2]);
  } else if (model.mode == PlayMode::Nearby && !model.flipBoard) {
    std::snprintf(northLabel, sizeof(northLabel), "NORTH (OPPONENT) RESERVE: G:%d  E:%d  C:%d",
                  game.reserves[northIdx][0], game.reserves[northIdx][1], game.reserves[northIdx][2]);
  } else {
    std::snprintf(northLabel, sizeof(northLabel), "NORTH RESERVE: G:%d  E:%d  C:%d",
                  game.reserves[northIdx][0], game.reserves[northIdx][1], game.reserves[northIdx][2]);
  }
  fui::TextStyle trayHeaderStyle;
  trayHeaderStyle.font = toybox::kTileFont;
  trayHeaderStyle.color = fui::Color::Black;
  trayHeaderStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(content.x, content.y, content.width, 22), northLabel, trayHeaderStyle);

  // Status banner if game over or AI thinking
  if (game.isOver) {
    char statusBuf[64];
    if (game.winner == model.humanPlayer) {
      std::snprintf(statusBuf, sizeof(statusBuf), "*** YOU CAUGHT THE LION! VICTORY! ***");
    } else if (game.winner == dobutsu::opponent(model.humanPlayer) && (model.mode == PlayMode::SoloVsAI || model.mode == PlayMode::Nearby)) {
      std::snprintf(statusBuf, sizeof(statusBuf), "%s Caught the Lion! Better luck next time.",
                    model.mode == PlayMode::Nearby ? "Opponent" : "AI");
    } else {
      std::snprintf(statusBuf, sizeof(statusBuf), "Player %d Won!", static_cast<int>(game.winner));
    }
    screen.target().text(fui::makeRect(content.x, static_cast<int16_t>(content.y + 26), content.width, 24),
                         statusBuf, trayHeaderStyle);
  } else if (model.isAiThinking) {
    screen.target().text(fui::makeRect(content.x, static_cast<int16_t>(content.y + 26), content.width, 24),
                         "AI is thinking...", trayHeaderStyle);
  } else if (model.mode == PlayMode::Nearby) {
    const bool yourTurn = game.turn == model.humanPlayer;
    screen.target().text(fui::makeRect(content.x, static_cast<int16_t>(content.y + 26), content.width, 24),
                         yourTurn ? "YOUR TURN" : "OPPONENT'S TURN", trayHeaderStyle);
  }

  // 3x4 Board Grid
  const fui::Rect grid = boardGridRect(content);
  screen.target().stroke(grid, fui::Paint::solid(fui::Color::Black), 2);

  // River line between row 1 and row 2
  const int16_t riverY = static_cast<int16_t>(grid.y + 2 * kTileH);
  screen.target().stroke(fui::makeRect(grid.x, static_cast<int16_t>(riverY - 1), grid.width, 2),
                         fui::Paint::solid(fui::Color::Black), 1);

  for (int8_t sq = 0; sq < dobutsu::kCells; ++sq) {
    const fui::Rect cBox = cellRect(grid, sq, model.flipBoard);
    screen.target().stroke(cBox, fui::Paint::solid(fui::Color::Black), 1);

    const dobutsu::Cell piece = game.board[sq];
    const bool isSelected = (model.selectedSq == sq);
    const bool isDest = isLegalDest(sq);

    // Draw piece if present
    drawPiece(screen, cBox, piece, isSelected, model.flipBoard);

    // If this is a legal target destination:
    if (isDest) {
      if (piece != 0) {
        // Capture indicator: inner corner marks
        screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 3),
                                             static_cast<int16_t>(cBox.y + 3),
                                             static_cast<int16_t>(cBox.width - 6),
                                             static_cast<int16_t>(cBox.height - 6)),
                               fui::Paint::solid(fui::Color::Black), 3);
      } else {
        // Move indicator: centered circle / target box
        const int16_t tSize = 16;
        screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + cBox.width / 2 - tSize / 2),
                                             static_cast<int16_t>(cBox.y + cBox.height / 2 - tSize / 2),
                                             tSize, tSize),
                               fui::Paint::solid(fui::Color::Black), 2);
      }
    }
  }

  // Bottom Area: South Reserve Tray & Interactive Drop Buttons
  const dobutsu::Player southPlayer = model.flipBoard ? dobutsu::Player::P2 : dobutsu::Player::P1;
  const int southIdx = (southPlayer == dobutsu::Player::P1) ? 0 : 1;

  const int16_t trayY = static_cast<int16_t>(grid.y + grid.height + 14);
  screen.target().text(fui::makeRect(content.x, trayY, content.width, 22),
                       "SOUTH RESERVE (TAP TO DROP):", trayHeaderStyle);

  const int16_t rBtnW = static_cast<int16_t>((kBoardW - 2 * toybox::kGutter) / 3);
  const int16_t rBtnH = 34;
  const int16_t rBtnsY = static_cast<int16_t>(trayY + 24);

  auto drawReserveButton = [&](size_t rIdx, int8_t dropConstant, const char* name, fui::ActionId action, int16_t x) {
    const uint8_t count = game.reserves[southIdx][rIdx];
    const bool isDropSelected = (model.selectedSq == dropConstant);

    char label[32];
    std::snprintf(label, sizeof(label), "%s: %d", name, count);

    fui::ButtonProps rBtn;
    rBtn.label = label;
    rBtn.action = action;
    const fui::Rect btnBox = fui::makeRect(x, rBtnsY, rBtnW, rBtnH);
    screen.button(rBtn, btnBox);

    if (isDropSelected) {
      screen.target().stroke(btnBox, fui::Paint::solid(fui::Color::Black), 3);
    }
  };

  const int16_t startX = grid.x;
  drawReserveButton(0, dobutsu::kDropGiraffe, "G", ActionReserveGiraffe, startX);
  drawReserveButton(1, dobutsu::kDropElephant, "E", ActionReserveElephant, static_cast<int16_t>(startX + rBtnW + toybox::kGutter));
  drawReserveButton(2, dobutsu::kDropChick, "C", ActionReserveChick, static_cast<int16_t>(startX + 2 * (rBtnW + toybox::kGutter)));
}

}  // namespace dobutsuui
