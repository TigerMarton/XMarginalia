#pragma once

#include "MancalaCore.h"

namespace mancala {

enum class Difficulty : uint8_t {
  Casual = 0,
  Standard,
  Master,
};

struct BestMove {
  int pit = -1;
  int score = 0;
};

BestMove findBestMove(const Game& game, uint8_t player, Difficulty diff = Difficulty::Standard);

}  // namespace mancala
