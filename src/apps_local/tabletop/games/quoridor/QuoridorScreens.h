#pragma once

#include <cstdint>
#include <FreeInkUI.h>

#include "../../../ui/ToyboxScreen.h"
#include "QuoridorCore.h"

namespace quoridorui {

namespace fui = freeink::ui;

// Action IDs 160..189
constexpr fui::ActionId ActionToggleMode = 160;
constexpr fui::ActionId ActionToggleOrient = 161;
constexpr fui::ActionId ActionConfirmWall = 162;
constexpr fui::ActionId ActionFlipBoard = 163;
constexpr fui::ActionId ActionRestart = 164;
constexpr fui::ActionId ActionBackToLobby = 165;

constexpr int16_t kBoardLeft = 54;
constexpr int16_t kBoardTop = 180;
constexpr int16_t kCellSize = 36;
constexpr int16_t kGrooveSize = 6;
constexpr int16_t kCellStep = kCellSize + kGrooveSize;  // 42
constexpr int16_t kBoardSize = 9 * kCellSize + 8 * kGrooveSize;  // 372

int8_t cellAt(int16_t tapX, int16_t tapY, bool flip);
bool grooveAt(int16_t tapX, int16_t tapY, int8_t& outGx, int8_t& outGy, bool flip);

void render(toybox::Screen& screen, const quoridor::State& state, uint8_t humanPlayer = 0);

}  // namespace quoridorui
