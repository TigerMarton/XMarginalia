#pragma once

#include "../../../ui/ToyboxScreen.h"
#include "../../TabletopFlow.h"
#include "OnitamaCore.h"

namespace onitamaui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRestart = 40,
  ActionBackToLobby = 41,
  ActionToggleMode = 42,
  ActionFlipBoard = 43,
};

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay,
  Nearby,
};

struct ScreenModel {
  onitama::Game game;
  PlayMode mode;
  uint8_t humanPlayer;
  int8_t selectedCell;  // -1 if none, 0..24
  int8_t selectedCard;  // 0 or 1
  bool isAiThinking;
  bool flipBoard;
};

void buildBoard(toybox::Screen& screen, const ScreenModel& model);
int cellAt(int x, int y, bool flipBoard);
int cardAt(int x, int y);

}  // namespace onitamaui
