#pragma once

#include "BackgammonCore.h"

namespace backgammon {

enum class Difficulty : uint8_t {
  Casual = 0,
  Standard = 1,
  Master = 2,
};

// Selects the single best move step for the active player
Move chooseBotMove(const Game& game, Difficulty diff, uint32_t seed);

}  // namespace backgammon
