#pragma once

#include "OnitamaCore.h"

namespace onitama {

enum class Difficulty : uint8_t {
  Casual = 2,
  Standard = 4,
  Master = 5,
};

Move chooseMove(const Game& game, Difficulty diff = Difficulty::Standard);

}  // namespace onitama
