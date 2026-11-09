#pragma once

#include "../../../ui/ToyboxScreen.h"
#include "ReversiCore.h"

namespace reversiui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRestart = 10,
  ActionPass = 11,
  ActionBackToLobby = 12,
  ActionToggleMode = 13,
  ActionFlipBoard = 14,
};

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay,
  Nearby,
};

struct BoardModel {
  reversi::Game game{};
  uint8_t humanSeat = reversi::kBlack;
  PlayMode mode = PlayMode::SoloVsAI;
  bool isAiThinking = false;
  bool flipBoard = false;
  uint64_t legalMoves = 0;
};

// Hit-testing
fui::Rect boardRect();
fui::Rect cellRect(int col, int row);
int cellAt(int x, int y, int& outCol, int& outRow, bool flipBoard = false);

void buildBoard(toybox::Screen& screen, const BoardModel& model);

}  // namespace reversiui
