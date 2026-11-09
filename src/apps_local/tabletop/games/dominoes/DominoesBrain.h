#pragma once

#include "DominoesCore.h"

namespace dominoes {

class DominoesBrain {
 public:
  static bool findBestMove(const State& state, Move& outMove);
  static bool shouldDraw(const State& state);
};

}  // namespace dominoes
