#include "QuoridorScreens.h"

#include <cstdio>

namespace quoridorui {

int8_t cellAt(int16_t tapX, int16_t tapY, bool flip) {
  if (tapX < kBoardLeft || tapX >= kBoardLeft + kBoardSize || tapY < kBoardTop || tapY >= kBoardTop + kBoardSize) {
    return -1;
  }
  int relX = tapX - kBoardLeft;
  int relY = tapY - kBoardTop;

  int col = relX / kCellStep;
  int row = relY / kCellStep;

  if (col < 0 || col >= quoridor::kCols || row < 0 || row >= quoridor::kRows) return -1;

  // Check if tap fell inside cell vs groove
  int inCellX = relX % kCellStep;
  int inCellY = relY % kCellStep;
  if (inCellX >= kCellSize || inCellY >= kCellSize) {
    // Fell inside groove
    return -1;
  }

  if (flip) {
    col = quoridor::kCols - 1 - col;
    row = quoridor::kRows - 1 - row;
  }
  return static_cast<int8_t>(quoridor::cellIdx(col, row));
}

bool grooveAt(int16_t tapX, int16_t tapY, int8_t& outGx, int8_t& outGy, bool flip) {
  if (tapX < kBoardLeft || tapX >= kBoardLeft + kBoardSize || tapY < kBoardTop || tapY >= kBoardTop + kBoardSize) {
    return false;
  }
  int relX = tapX - kBoardLeft;
  int relY = tapY - kBoardTop;

  // Intersections are centered around (gx * 42 + 36, gy * 42 + 36)
  // Hit-test with 20px tolerance
  for (int gy = 0; gy < quoridor::kInterRows; ++gy) {
    int iy = gy * kCellStep + kCellSize;
    if (relY >= iy - 14 && relY <= iy + kGrooveSize + 14) {
      for (int gx = 0; gx < quoridor::kInterCols; ++gx) {
        int ix = gx * kCellStep + kCellSize;
        if (relX >= ix - 14 && relX <= ix + kGrooveSize + 14) {
          int8_t resGx = static_cast<int8_t>(gx);
          int8_t resGy = static_cast<int8_t>(gy);
          if (flip) {
            resGx = static_cast<int8_t>(quoridor::kInterCols - 1 - resGx);
            resGy = static_cast<int8_t>(quoridor::kInterRows - 1 - resGy);
          }
          outGx = resGx;
          outGy = resGy;
          return true;
        }
      }
    }
  }
  return false;
}

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

void render(toybox::Screen& screen, const quoridor::State& state, uint8_t humanPlayer) {
  // 1. Header
  const char* modeLabel = (state.mode == quoridor::Mode::Nearby)
                              ? "NEARBY LINK"
                              : ((state.mode == quoridor::Mode::Solo) ? "SOLO VS AI" : "PASS & PLAY");
  toyboxChrome(screen, "QUORIDOR", modeLabel);

  // 2. Headline & Status Banner
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

  if (state.winner != quoridor::Player::None) {
    if (state.mode == quoridor::Mode::Nearby) {
      const bool won = state.winner == static_cast<quoridor::Player>(humanPlayer);
      std::snprintf(headline, sizeof(headline), "%s", won ? "YOU WIN!" : "OPPONENT WINS!");
    } else if (state.winner == quoridor::Player::White) {
      std::snprintf(headline, sizeof(headline), "WHITE WINS!");
    } else {
      std::snprintf(headline, sizeof(headline), "BLACK WINS!");
    }
    std::snprintf(subline, sizeof(subline), "Tap Restart to play again");
  } else {
    if (state.mode == quoridor::Mode::Nearby) {
      const bool yourTurn = state.turn == static_cast<quoridor::Player>(humanPlayer);
      std::snprintf(headline, sizeof(headline), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
      if (yourTurn) {
        if (state.selectedGx != -1) {
          std::snprintf(subline, sizeof(subline), "Wall previewed - tap Place Wall to confirm");
        } else {
          std::snprintf(subline, sizeof(subline), "Tap square to move pawn, or groove to place wall");
        }
      } else {
        std::snprintf(subline, sizeof(subline), "Waiting for opponent's move...");
      }
    } else {
      if (state.turn == quoridor::Player::White) {
        std::snprintf(headline, sizeof(headline), "%s",
                      (state.mode == quoridor::Mode::Solo) ? "YOUR TURN (WHITE)" : "WHITE'S TURN");
      } else {
        std::snprintf(headline, sizeof(headline), "%s",
                      (state.mode == quoridor::Mode::Solo) ? "AI THINKING..." : "BLACK'S TURN");
      }

      if (state.selectedGx != -1) {
        std::snprintf(subline, sizeof(subline), "Wall previewed - tap Place Wall to confirm");
      } else {
        std::snprintf(subline, sizeof(subline), "Tap square to move pawn, or groove to place wall");
      }
    }
  }

  screen.target().text(fui::makeRect(16, 88, 448, 22), headline, headlineStyle);
  screen.target().text(fui::makeRect(16, 124, 448, 18), subline, sublineStyle);

  // Wall counts pill
  char wallCountBuf[64];
  std::snprintf(wallCountBuf, sizeof(wallCountBuf), "WALLS LEFT:  WHITE %d  |  BLACK %d",
                state.wallsLeft[0], state.wallsLeft[1]);
  screen.target().text(fui::makeRect(16, 152, 448, 18), wallCountBuf, sublineStyle);

  // 3. Board Outer Border
  const fui::Rect boardBox =
      fui::makeRect(static_cast<int16_t>(kBoardLeft - 2), static_cast<int16_t>(kBoardTop - 2),
                    static_cast<int16_t>(kBoardSize + 4), static_cast<int16_t>(kBoardSize + 4));
  screen.target().stroke(boardBox, fui::Paint::solid(fui::Color::Black), 2);

  // Pre-calculate legal pawn moves
  int8_t pawnMoves[8];
  int pCount = 0;
  if (state.winner == quoridor::Player::None) {
    pCount = quoridor::getLegalPawnMoves(state, pawnMoves, 8);
  }

  // Draw 9x9 cells
  for (int8_t r = 0; r < quoridor::kRows; ++r) {
    for (int8_t c = 0; c < quoridor::kCols; ++c) {
      int8_t bCol = state.flip ? static_cast<int8_t>(quoridor::kCols - 1 - c) : c;
      int8_t bRow = state.flip ? static_cast<int8_t>(quoridor::kRows - 1 - r) : r;
      int8_t cell = static_cast<int8_t>(quoridor::cellIdx(bCol, bRow));

      const fui::Rect cBox =
          fui::makeRect(static_cast<int16_t>(kBoardLeft + c * kCellStep),
                        static_cast<int16_t>(kBoardTop + r * kCellStep), kCellSize, kCellSize);

      screen.target().stroke(cBox, fui::Paint::solid(fui::Color::Black), 1);

      // Check for White or Black pawn
      if (cell == state.pawn[0]) {
        // White pawn: white filled circle with black border
        const int16_t midX = static_cast<int16_t>(cBox.x + kCellSize / 2);
        const int16_t midY = static_cast<int16_t>(cBox.y + kCellSize / 2);
        screen.target().fill(fui::makeRect(midX - 12, midY - 12, 24, 24), fui::Paint::solid(fui::Color::White));
        screen.target().stroke(fui::makeRect(midX - 12, midY - 12, 24, 24), fui::Paint::solid(fui::Color::Black), 2);

        fui::TextStyle pStyle;
        pStyle.font = toybox::kTileFont;
        pStyle.color = fui::Color::Black;
        pStyle.align = fui::TextAlign::Center;
        screen.target().text(fui::makeRect(midX - 12, midY - 6, 24, 14), "W", pStyle);
      } else if (cell == state.pawn[1]) {
        // Black pawn: solid black circle with white text
        const int16_t midX = static_cast<int16_t>(cBox.x + kCellSize / 2);
        const int16_t midY = static_cast<int16_t>(cBox.y + kCellSize / 2);
        screen.target().fill(fui::makeRect(midX - 12, midY - 12, 24, 24), fui::Paint::solid(fui::Color::Black));

        fui::TextStyle pStyle;
        pStyle.font = toybox::kTileFont;
        pStyle.color = fui::Color::White;
        pStyle.align = fui::TextAlign::Center;
        screen.target().text(fui::makeRect(midX - 12, midY - 6, 24, 14), "B", pStyle);
      } else {
        // Check if legal move target
        bool isMoveTarget = false;
        for (int i = 0; i < pCount; ++i) {
          if (pawnMoves[i] == cell) {
            isMoveTarget = true;
            break;
          }
        }
        if (isMoveTarget) {
          const int16_t midX = static_cast<int16_t>(cBox.x + kCellSize / 2);
          const int16_t midY = static_cast<int16_t>(cBox.y + kCellSize / 2);
          screen.target().fill(fui::makeRect(midX - 4, midY - 4, 8, 8), fui::Paint::solid(fui::Color::Black));
        }
      }
    }
  }

  // Draw Placed Walls
  for (int8_t gy = 0; gy < quoridor::kInterRows; ++gy) {
    for (int8_t gx = 0; gx < quoridor::kInterCols; ++gx) {
      int8_t vGx = state.flip ? static_cast<int8_t>(quoridor::kInterCols - 1 - gx) : gx;
      int8_t vGy = state.flip ? static_cast<int8_t>(quoridor::kInterRows - 1 - gy) : gy;

      if (quoridor::hasHWall(state.hWalls, vGx, vGy)) {
        int16_t wx = static_cast<int16_t>(kBoardLeft + (state.flip ? (quoridor::kInterCols - 1 - gx) : gx) * kCellStep);
        int16_t wy = static_cast<int16_t>(kBoardTop + (state.flip ? (quoridor::kInterRows - 1 - gy) : gy) * kCellStep + kCellSize);
        screen.target().fill(fui::makeRect(wx, wy, static_cast<int16_t>(2 * kCellSize + kGrooveSize), kGrooveSize),
                             fui::Paint::solid(fui::Color::Black));
      }
      if (quoridor::hasVWall(state.vWalls, vGx, vGy)) {
        int16_t wx = static_cast<int16_t>(kBoardLeft + (state.flip ? (quoridor::kInterCols - 1 - gx) : gx) * kCellStep + kCellSize);
        int16_t wy = static_cast<int16_t>(kBoardTop + (state.flip ? (quoridor::kInterRows - 1 - gy) : gy) * kCellStep);
        screen.target().fill(fui::makeRect(wx, wy, kGrooveSize, static_cast<int16_t>(2 * kCellSize + kGrooveSize)),
                             fui::Paint::solid(fui::Color::Black));
      }
    }
  }

  // Draw Selected Wall Preview
  if (state.selectedGx != -1 && state.selectedGy != -1) {
    int8_t pgx = state.flip ? static_cast<int8_t>(quoridor::kInterCols - 1 - state.selectedGx) : state.selectedGx;
    int8_t pgy = state.flip ? static_cast<int8_t>(quoridor::kInterRows - 1 - state.selectedGy) : state.selectedGy;

    if (state.curOrient == quoridor::WallOrientation::Horizontal) {
      int16_t wx = static_cast<int16_t>(kBoardLeft + pgx * kCellStep);
      int16_t wy = static_cast<int16_t>(kBoardTop + pgy * kCellStep + kCellSize - 1);
      screen.target().stroke(fui::makeRect(wx, wy, static_cast<int16_t>(2 * kCellSize + kGrooveSize),
                                           static_cast<int16_t>(kGrooveSize + 2)),
                             fui::Paint::solid(fui::Color::Black), 2);
    } else {
      int16_t wx = static_cast<int16_t>(kBoardLeft + pgx * kCellStep + kCellSize - 1);
      int16_t wy = static_cast<int16_t>(kBoardTop + pgy * kCellStep);
      screen.target().stroke(fui::makeRect(wx, wy, static_cast<int16_t>(kGrooveSize + 2),
                                           static_cast<int16_t>(2 * kCellSize + kGrooveSize)),
                             fui::Paint::solid(fui::Color::Black), 2);
    }
  }

  // 4. Instructions / Goal Hint
  fui::TextStyle goalStyle;
  goalStyle.font = toybox::kTileFont;
  goalStyle.color = fui::Color::Black;
  goalStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(16, 556, 448, 18), "Goal: Reach opposite side of the board first", goalStyle);

  // 5. Action Buttons (2 rows of 2 buttons, 216px each)
  const int16_t row1Y = 660;

  if (state.selectedGx != -1) {
    fui::ButtonProps placeBtn;
    placeBtn.label = "PLACE WALL";
    placeBtn.action = ActionConfirmWall;
    screen.button(placeBtn, fui::makeRect(16, row1Y, 216, 44));
  } else {
    char modeBtnText[32];
    std::snprintf(modeBtnText, sizeof(modeBtnText), "MODE: %s",
                  (state.mode == quoridor::Mode::Nearby) ? "NEARBY"
                                                         : ((state.mode == quoridor::Mode::Solo) ? "SOLO" : "2P"));
    fui::ButtonProps modeBtn;
    modeBtn.label = modeBtnText;
    modeBtn.action = ActionToggleMode;
    screen.button(modeBtn, fui::makeRect(16, row1Y, 216, 44));
  }

  char orientLabel[32];
  std::snprintf(orientLabel, sizeof(orientLabel), "WALL: %s",
                (state.curOrient == quoridor::WallOrientation::Horizontal) ? "HORIZ" : "VERT");
  fui::ButtonProps orientBtn;
  orientBtn.label = orientLabel;
  orientBtn.action = ActionToggleOrient;
  screen.button(orientBtn, fui::makeRect(248, row1Y, 216, 44));

  const int16_t row2Y = 716;
  fui::ButtonProps rstBtn;
  rstBtn.label = "RESTART";
  rstBtn.action = ActionRestart;
  screen.button(rstBtn, fui::makeRect(16, row2Y, 216, 44));

  fui::ButtonProps lobbyBtn;
  lobbyBtn.label = "LOBBY";
  lobbyBtn.action = ActionBackToLobby;
  screen.button(lobbyBtn, fui::makeRect(248, row2Y, 216, 44));
}

}  // namespace quoridorui
