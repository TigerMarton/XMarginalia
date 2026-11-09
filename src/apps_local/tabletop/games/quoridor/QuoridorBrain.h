#pragma once

#include "QuoridorCore.h"

namespace quoridor {

class QuoridorBrain {
 public:
  static bool findBestMove(const State& state, Move& outMove, int depth = 2);
  static int evaluate(const State& state, Player aiPlayer);
};

}  // namespace quoridor
