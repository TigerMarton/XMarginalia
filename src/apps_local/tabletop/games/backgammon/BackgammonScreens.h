#pragma once

#include <cstdint>
#include <FreeInkUI.h>
#include "../../../ui/ToyboxScreen.h"
#include "BackgammonCore.h"

namespace backgammonui {

namespace fui = freeink::ui;

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay = 1,
  Nearby = 2,
};

// Action IDs 110..139
constexpr fui::ActionId ActionRollDice = 110;
constexpr fui::ActionId ActionEndTurn = 111;
constexpr fui::ActionId ActionBearOff = 112;
constexpr fui::ActionId ActionRestart = 113;
constexpr fui::ActionId ActionToggleMode = 114;
constexpr fui::ActionId ActionFlipBoard = 115;
constexpr fui::ActionId ActionBackToLobby = 116;

struct ScreenModel {
  backgammon::Game game;
  backgammon::Player humanPlayer = backgammon::Player::White;
  PlayMode mode = PlayMode::SoloVsAI;
  int8_t selectedPoint = -99;  // 0..23, or kFromBar (-1), or -99 (none)
  bool isAiThinking = false;
  bool flipBoard = false;
};

void buildBoard(toybox::Screen& screen, const ScreenModel& model);

// Geometry helpers
int8_t pointAt(int x, int y, bool flip);
fui::Rect boardRect();
fui::Rect barRect();
fui::Rect bearOffRect(backgammon::Player p, bool flip);

}  // namespace backgammonui
