#pragma once

#include "../../../ui/ToyboxScreen.h"
#include "MorrisCore.h"

namespace morrisui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRestart = 30,
  ActionBackToLobby = 31,
  ActionToggleMode = 32,
  ActionFlipBoard = 33,
};

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay,
  Nearby,
};

struct BoardModel {
  morris::Game game{};
  uint8_t humanPlayer = morris::kWhite;
  PlayMode mode = PlayMode::SoloVsAI;
  bool isAiThinking = false;
  bool flipBoard = false;
};

// Hit-testing
fui::Rect boardRect();
fui::Point vertexPos(int v, bool flipBoard = false);
int vertexAt(int x, int y, bool flipBoard = false);

void buildBoard(toybox::Screen& screen, const BoardModel& model);

}  // namespace morrisui
