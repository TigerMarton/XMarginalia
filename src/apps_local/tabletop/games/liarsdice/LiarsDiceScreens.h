#pragma once

#include "../../../ui/ToyboxScreen.h"
#include "../../TabletopFlow.h"
#include "LiarsDiceCore.h"

namespace liarsdiceui {

namespace fui = freeink::ui;

enum : fui::ActionId {
  ActionRestart = 60,
  ActionBackToLobby = 61,
  ActionToggleMode = 62,
  ActionFlipBoard = 63,
  ActionNextRound = 64,
  ActionChallenge = 65,
  ActionMakeBid = 66,
  ActionCountMinus = 67,
  ActionCountPlus = 68,
  ActionFaceBase = 70,  // 70..75 for face values 1..6
  ActionTogglePeek = 76,
};

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay,
  Nearby,
};

struct ScreenModel {
  liarsdice::Game game;
  PlayMode mode = PlayMode::SoloVsAI;
  uint8_t humanPlayer = 0;
  uint8_t selectedCount = 1;
  uint8_t selectedFace = 2;
  bool isAiThinking = false;
  bool shieldDice = false;
  bool flipBoard = false;
  bool isYourTurn = true;
};

void buildBoard(toybox::Screen& screen, const ScreenModel& model);

}  // namespace liarsdiceui
