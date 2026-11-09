#pragma once

#include "SantoriniCore.h"

namespace santorini {

class SantoriniBrain {
 public:
  static bool findBestMove(const State& state, Move& outMove, int depth = 3);
  static int evaluate(const State& state, Player aiPlayer);
};

}  // namespace santorini
