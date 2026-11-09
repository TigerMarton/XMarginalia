#pragma once

#include "ReversiCore.h"

namespace reversi {

enum class Difficulty : uint8_t {
  Casual = 0,
  Standard,
  Master,
};

struct BestMove {
  int col = -1;
  int row = -1;
  int score = 0;
};

BestMove findBestMove(const Game& game, uint8_t player, Difficulty diff = Difficulty::Standard);

}  // namespace reversi
