#pragma once

#include "../../../ui/ToyboxScreen.h"
#include "MancalaCore.h"

namespace mancalaui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRestart = 20,
  ActionBackToLobby = 21,
  ActionToggleMode = 22,
  ActionFlipBoard = 23,
};

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay,
  Nearby,
};

struct BoardModel {
  mancala::Game game{};
  uint8_t humanPlayer = mancala::kPlayer1;
  PlayMode mode = PlayMode::SoloVsAI;
  bool isAiThinking = false;
  bool flipBoard = false;
};

// Hit-testing
fui::Rect boardRect();
fui::Rect pitRect(int pit, bool flipBoard = false);
int pitAt(int x, int y, bool flipBoard = false);

void buildBoard(toybox::Screen& screen, const BoardModel& model);

}  // namespace mancalaui
