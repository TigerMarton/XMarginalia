#pragma once

#include "DobutsuCore.h"

namespace dobutsu {

enum class Difficulty : uint8_t {
  Casual = 0,
  Standard = 1,
  Master = 2,
};

Move chooseBotMove(const Game& game, Difficulty diff, uint32_t seed);

}  // namespace dobutsu
