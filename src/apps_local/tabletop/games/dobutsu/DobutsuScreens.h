#pragma once

#include <cstdint>
#include <FreeInkUI.h>
#include "../../../ui/ToyboxScreen.h"
#include "DobutsuCore.h"

namespace dobutsuui {

namespace fui = freeink::ui;

enum class PlayMode : uint8_t {
  SoloVsAI = 0,
  TwoPlayerPassAndPlay = 1,
  Nearby = 2,
};

// Action IDs 80..99
constexpr fui::ActionId ActionCellBase = 80;         // 80..91 for board cells 0..11
constexpr fui::ActionId ActionReserveGiraffe = 92;
constexpr fui::ActionId ActionReserveElephant = 93;
constexpr fui::ActionId ActionReserveChick = 94;
constexpr fui::ActionId ActionRestart = 95;
constexpr fui::ActionId ActionToggleMode = 96;
constexpr fui::ActionId ActionFlipBoard = 97;
constexpr fui::ActionId ActionBackToLobby = 98;

struct ScreenModel {
  dobutsu::Game game;
  dobutsu::Player humanPlayer = dobutsu::Player::P1;
  PlayMode mode = PlayMode::SoloVsAI;
  int8_t selectedSq = -1;  // 0..11 for board piece, or kDropGiraffe / kDropElephant / kDropChick
  bool isAiThinking = false;
  bool flipBoard = false;
};

void buildBoard(toybox::Screen& screen, const ScreenModel& model);

// Geometry helpers for touch hit-testing
int cellAt(int x, int y, bool flip);
fui::Rect boardGridRect(const fui::Rect& content);
fui::Rect cellRect(const fui::Rect& grid, int8_t sq, bool flip);

}  // namespace dobutsuui
