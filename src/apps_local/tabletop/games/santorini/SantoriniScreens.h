#pragma once

#include <cstdint>
#include <FreeInkUI.h>

#include "../../../ui/ToyboxScreen.h"
#include "SantoriniCore.h"

namespace santoriniui {

namespace fui = freeink::ui;

// Action IDs 140..159
constexpr fui::ActionId ActionToggleMode = 140;
constexpr fui::ActionId ActionFlipBoard = 141;
constexpr fui::ActionId ActionRestart = 142;
constexpr fui::ActionId ActionBackToLobby = 143;

constexpr int16_t kBoardLeft = 65;
constexpr int16_t kBoardTop = 160;
constexpr int16_t kCellSize = 70;
constexpr int16_t kBoardSize = 350;

int8_t cellAt(int16_t tapX, int16_t tapY, bool flip);

void render(toybox::Screen& screen, const santorini::State& state, uint8_t humanPlayer = 0);

}  // namespace santoriniui
