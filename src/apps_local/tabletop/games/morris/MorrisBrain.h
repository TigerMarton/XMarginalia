#pragma once

#include "MorrisCore.h"

namespace morris {

enum class Difficulty : uint8_t {
  Casual = 0,
  Standard,
  Master,
};

struct BestMove {
  enum Kind : uint8_t { None, Place, Slide, Remove };
  Kind kind = None;
  int8_t from = -1;
  int8_t to = -1;
  int score = 0;
};

BestMove findBestMove(const Game& game, uint8_t player, Difficulty diff = Difficulty::Standard);

}  // namespace morris
