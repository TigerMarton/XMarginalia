#pragma once

#include <cstdint>
#include <FreeInkUI.h>

#include "../../../ui/ToyboxScreen.h"
#include "DominoesCore.h"

namespace dominoesui {

namespace fui = freeink::ui;

// Action IDs 190..219
constexpr fui::ActionId ActionPlayLeft = 190;
constexpr fui::ActionId ActionPlayRight = 191;
constexpr fui::ActionId ActionDrawTile = 192;
constexpr fui::ActionId ActionPassTurn = 193;
constexpr fui::ActionId ActionTogglePrivacy = 194;
constexpr fui::ActionId ActionToggleMode = 195;
constexpr fui::ActionId ActionRestart = 196;
constexpr fui::ActionId ActionBackToLobby = 197;

constexpr int16_t kHandAreaY = 400;
constexpr int16_t kTileWidth = 48;
constexpr int16_t kTileHeight = 84;

int8_t handIdxAt(int16_t tapX, int16_t tapY, int handCount);

void render(toybox::Screen& screen, const dominoes::State& state, uint8_t humanPlayer = 0);

}  // namespace dominoesui
