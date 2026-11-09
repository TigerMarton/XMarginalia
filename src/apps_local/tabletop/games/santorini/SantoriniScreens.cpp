#include "SantoriniScreens.h"

#include <cstdio>

namespace santoriniui {

int8_t cellAt(int16_t tapX, int16_t tapY, bool flip) {
  if (tapX < kBoardLeft || tapX >= kBoardLeft + kBoardSize || tapY < kBoardTop || tapY >= kBoardTop + kBoardSize) {
    return -1;
  }
  int8_t col = static_cast<int8_t>((tapX - kBoardLeft) / kCellSize);
  int8_t row = static_cast<int8_t>((tapY - kBoardTop) / kCellSize);
  if (col < 0 || col >= santorini::kCols || row < 0 || row >= santorini::kRows) {
    return -1;
  }
  if (flip) {
    col = static_cast<int8_t>(santorini::kCols - 1 - col);
    row = static_cast<int8_t>(santorini::kRows - 1 - row);
  }
  return static_cast<int8_t>(santorini::cellIdx(col, row));
}

namespace {

void drawCell(toybox::Screen& screen, const fui::Rect& cBox, uint8_t level, int wIdx, santorini::Player owner,
              bool isSelected, bool isTarget, bool isBuild) {
  // 1. Base cell border
  screen.target().stroke(cBox, fui::Paint::solid(fui::Color::Black), 1);

  // 2. Concentric ring geometry for 3D building levels
  if (level >= 1) {
    screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 6), static_cast<int16_t>(cBox.y + 6),
                                         static_cast<int16_t>(kCellSize - 12), static_cast<int16_t>(kCellSize - 12)),
                           fui::Paint::solid(fui::Color::Black), 2);
  }
  if (level >= 2) {
    screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 14), static_cast<int16_t>(cBox.y + 14),
                                         static_cast<int16_t>(kCellSize - 28), static_cast<int16_t>(kCellSize - 28)),
                           fui::Paint::solid(fui::Color::Black), 2);
  }
  if (level >= 3) {
    const fui::Rect l3Box = fui::makeRect(static_cast<int16_t>(cBox.x + 21), static_cast<int16_t>(cBox.y + 21),
                                          static_cast<int16_t>(kCellSize - 42), static_cast<int16_t>(kCellSize - 42));
    screen.target().fill(l3Box, fui::Paint::solid(fui::Color::White));
    screen.target().stroke(l3Box, fui::Paint::solid(fui::Color::Black), 3);
  }
  if (level >= 4) {
    // Dome: Solid black box with white cross knockout
    const fui::Rect dBox = fui::makeRect(static_cast<int16_t>(cBox.x + 20), static_cast<int16_t>(cBox.y + 20), 30, 30);
    screen.target().fill(dBox, fui::Paint::solid(fui::Color::Black));
    screen.target().fill(fui::makeRect(static_cast<int16_t>(dBox.x + 5), static_cast<int16_t>(dBox.y + 13), 20, 4),
                         fui::Paint::solid(fui::Color::White));
    screen.target().fill(fui::makeRect(static_cast<int16_t>(dBox.x + 13), static_cast<int16_t>(dBox.y + 5), 4, 20),
                         fui::Paint::solid(fui::Color::White));
  }

  // 3. Selection outline
  if (isSelected) {
    screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 2), static_cast<int16_t>(cBox.y + 2),
                                         static_cast<int16_t>(kCellSize - 4), static_cast<int16_t>(kCellSize - 4)),
                           fui::Paint::solid(fui::Color::Black), 3);
  }

  // 4. Target indicators
  if (isTarget) {
    if (isBuild) {
      // Build cross indicator
      screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 23), static_cast<int16_t>(cBox.y + 23), 24, 24),
                             fui::Paint::solid(fui::Color::Black), 2);
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cBox.x + 27), static_cast<int16_t>(cBox.y + 33), 16, 4),
                           fui::Paint::solid(fui::Color::Black));
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cBox.x + 33), static_cast<int16_t>(cBox.y + 27), 4, 16),
                           fui::Paint::solid(fui::Color::Black));
    } else {
      // Move target indicator
      screen.target().stroke(fui::makeRect(static_cast<int16_t>(cBox.x + 24), static_cast<int16_t>(cBox.y + 24), 22, 22),
                             fui::Paint::solid(fui::Color::Black), 2);
      screen.target().fill(fui::makeRect(static_cast<int16_t>(cBox.x + 32), static_cast<int16_t>(cBox.y + 32), 6, 6),
                           fui::Paint::solid(fui::Color::Black));
    }
  }

  // 5. Worker
  if (wIdx != -1) {
    const fui::Rect wBox = fui::makeRect(static_cast<int16_t>(cBox.x + 19), static_cast<int16_t>(cBox.y + 19), 32, 32);
    char badge[16];
    std::snprintf(badge, sizeof(badge), "%s%d", (owner == santorini::Player::White) ? "W" : "B", wIdx + 1);

    fui::TextStyle textStyle;
    textStyle.font = toybox::kTileFont;
    textStyle.align = fui::TextAlign::Center;

    if (owner == santorini::Player::White) {
      screen.target().fill(wBox, fui::Paint::solid(fui::Color::White));
      screen.target().stroke(wBox, fui::Paint::solid(fui::Color::Black), 2);
      textStyle.color = fui::Color::Black;
      screen.target().text(fui::makeRect(wBox.x, static_cast<int16_t>(wBox.y + 9), wBox.width, 14), badge, textStyle);
    } else {
      screen.target().fill(wBox, fui::Paint::solid(fui::Color::Black));
      textStyle.color = fui::Color::White;
      screen.target().text(fui::makeRect(wBox.x, static_cast<int16_t>(wBox.y + 9), wBox.width, 14), badge, textStyle);
    }
  }
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

void render(toybox::Screen& screen, const santorini::State& state, uint8_t humanPlayer) {
  // 1. Header
  const char* modeLabel = (state.mode == santorini::Mode::Nearby)
                              ? "NEARBY LINK"
                              : ((state.mode == santorini::Mode::Solo) ? "SOLO VS AI" : "PASS & PLAY");
  toyboxChrome(screen, "SANTORINI", modeLabel);

  // 2. Status & Instructions Banner
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

  if (state.phase == santorini::Phase::GameOver) {
    if (state.mode == santorini::Mode::Nearby) {
      const bool won = state.winner == static_cast<santorini::Player>(humanPlayer);
      std::snprintf(headline, sizeof(headline), "%s", won ? "YOU WIN!" : "OPPONENT WINS!");
    } else if (state.winner == santorini::Player::White) {
      std::snprintf(headline, sizeof(headline), "WHITE WINS!");
    } else if (state.winner == santorini::Player::Black) {
      std::snprintf(headline, sizeof(headline), "BLACK WINS!");
    } else {
      std::snprintf(headline, sizeof(headline), "GAME OVER");
    }
    std::snprintf(subline, sizeof(subline), "Tap Restart to play again");
  } else {
    if (state.mode == santorini::Mode::Nearby) {
      const bool yourTurn = state.turn == static_cast<santorini::Player>(humanPlayer);
      std::snprintf(headline, sizeof(headline), "%s", yourTurn ? "YOUR TURN" : "OPPONENT'S TURN");
      if (yourTurn) {
        if (state.phase == santorini::Phase::SelectWorker) {
          std::snprintf(subline, sizeof(subline), "Tap a worker to move");
        } else if (state.phase == santorini::Phase::SelectMove) {
          std::snprintf(subline, sizeof(subline), "Tap adjacent space to move");
        } else if (state.phase == santorini::Phase::SelectBuild) {
          std::snprintf(subline, sizeof(subline), "Tap adjacent space to build");
        } else {
          subline[0] = '\0';
        }
      } else {
        std::snprintf(subline, sizeof(subline), "Waiting for opponent's move...");
      }
    } else {
      if (state.turn == santorini::Player::White) {
        std::snprintf(headline, sizeof(headline), "%s",
                      (state.mode == santorini::Mode::Solo) ? "YOUR TURN (WHITE)" : "WHITE'S TURN");
      } else {
        std::snprintf(headline, sizeof(headline), "%s",
                      (state.mode == santorini::Mode::Solo) ? "AI THINKING..." : "BLACK'S TURN");
      }

      if (state.phase == santorini::Phase::SelectWorker) {
        std::snprintf(subline, sizeof(subline), "Tap a worker to move");
      } else if (state.phase == santorini::Phase::SelectMove) {
        std::snprintf(subline, sizeof(subline), "Tap adjacent space to move");
      } else if (state.phase == santorini::Phase::SelectBuild) {
        std::snprintf(subline, sizeof(subline), "Tap adjacent space to build");
      } else {
        subline[0] = '\0';
      }
    }
  }

  screen.target().text(fui::makeRect(16, 92, 448, 24), headline, headlineStyle);
  if (subline[0] != '\0') {
    screen.target().text(fui::makeRect(16, 126, 448, 20), subline, sublineStyle);
  }

  // 3. Board Outer Border
  const fui::Rect boardBox =
      fui::makeRect(static_cast<int16_t>(kBoardLeft - 2), static_cast<int16_t>(kBoardTop - 2),
                    static_cast<int16_t>(kBoardSize + 4), static_cast<int16_t>(kBoardSize + 4));
  screen.target().stroke(boardBox, fui::Paint::solid(fui::Color::Black), 2);

  // Pre-calculate legal move/build targets
  int8_t legalTargets[8];
  int targetCount = 0;
  if (state.phase == santorini::Phase::SelectMove && state.selectedWorker != -1) {
    targetCount = santorini::getLegalMovesForWorker(state, state.selectedWorker, legalTargets, 8);
  } else if (state.phase == santorini::Phase::SelectBuild) {
    targetCount = santorini::getLegalBuilds(state, legalTargets, 8);
  }

  for (int8_t r = 0; r < santorini::kRows; ++r) {
    for (int8_t c = 0; c < santorini::kCols; ++c) {
      int8_t boardCol = state.flip ? static_cast<int8_t>(santorini::kCols - 1 - c) : c;
      int8_t boardRow = state.flip ? static_cast<int8_t>(santorini::kRows - 1 - r) : r;
      int8_t cell = static_cast<int8_t>(santorini::cellIdx(boardCol, boardRow));

      const fui::Rect cBox =
          fui::makeRect(static_cast<int16_t>(kBoardLeft + c * kCellSize),
                        static_cast<int16_t>(kBoardTop + r * kCellSize), kCellSize, kCellSize);

      santorini::Player owner = santorini::Player::None;
      int wIdx = santorini::workerAt(state, cell, owner);

      bool isSelected = (owner == state.turn && wIdx == state.selectedWorker && state.selectedWorker != -1);

      bool isTarget = false;
      for (int t = 0; t < targetCount; ++t) {
        if (legalTargets[t] == cell) {
          isTarget = true;
          break;
        }
      }

      drawCell(screen, cBox, state.board[cell], wIdx, owner, isSelected, isTarget,
               state.phase == santorini::Phase::SelectBuild);
    }
  }

  // 4. Legend
  fui::TextStyle legendStyle;
  legendStyle.font = toybox::kTileFont;
  legendStyle.color = fui::Color::Black;
  legendStyle.align = fui::TextAlign::Center;
  screen.target().text(fui::makeRect(16, 532, 448, 18), "[0] Ground   [1] L1   [2] L2   [3] L3   [D] Dome", legendStyle);
  screen.target().text(fui::makeRect(16, 554, 448, 18), "Win: Step onto Level 3, or trap opponent", legendStyle);

  // 5. Action Buttons (2 rows of 2 buttons, 216px each)
  const int16_t row1Y = 660;
  char modeBtnText[32];
  std::snprintf(modeBtnText, sizeof(modeBtnText), "MODE: %s",
                (state.mode == santorini::Mode::Nearby) ? "NEARBY"
                                                        : ((state.mode == santorini::Mode::Solo) ? "SOLO" : "2P"));

  fui::ButtonProps modeBtn;
  modeBtn.label = modeBtnText;
  modeBtn.action = ActionToggleMode;
  screen.button(modeBtn, fui::makeRect(16, row1Y, 216, 44));

  fui::ButtonProps flipBtn;
  flipBtn.label = "FLIP BOARD";
  flipBtn.action = ActionFlipBoard;
  screen.button(flipBtn, fui::makeRect(248, row1Y, 216, 44));

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

}  // namespace santoriniui
