#include "DominoesBrain.h"

namespace dominoes {

bool DominoesBrain::shouldDraw(const State& state) {
  if (state.roundOver) return false;
  return !hasAnyLegalPlay(state, state.turn) && state.boneyardCount > 0;
}

bool DominoesBrain::findBestMove(const State& state, Move& outMove) {
  if (state.roundOver) return false;

  int pIdx = static_cast<int>(state.turn);
  int bestScore = -10000;
  bool found = false;

  // Count suits in bot's hand for suit control
  int suitCount[7] = {0};
  for (int i = 0; i < state.handCount[pIdx]; ++i) {
    int8_t tIdx = state.hand[pIdx][i];
    suitCount[kTiles[tIdx].pips[0]]++;
    suitCount[kTiles[tIdx].pips[1]]++;
  }

  for (int i = 0; i < state.handCount[pIdx]; ++i) {
    int8_t tIdx = state.hand[pIdx][i];
    const auto& t = kTiles[tIdx];
    int pipSum = t.pips[0] + t.pips[1];
    bool isDouble = (t.pips[0] == t.pips[1]);

    // Check Left Play
    if (canPlayLeft(state, tIdx)) {
      int8_t exposedPip = (t.pips[1] == state.leftEnd) ? t.pips[0] : t.pips[1];
      int score = pipSum * 5 + (isDouble ? 25 : 0) + suitCount[exposedPip] * 12;

      if (!found || score > bestScore) {
        bestScore = score;
        outMove.handIdx = static_cast<int8_t>(i);
        outMove.side = PlaySide::Left;
        found = true;
      }
    }

    // Check Right Play
    if (canPlayRight(state, tIdx)) {
      int8_t exposedPip = (t.pips[0] == state.rightEnd) ? t.pips[1] : t.pips[0];
      int score = pipSum * 5 + (isDouble ? 25 : 0) + suitCount[exposedPip] * 12;

      if (!found || score > bestScore) {
        bestScore = score;
        outMove.handIdx = static_cast<int8_t>(i);
        outMove.side = PlaySide::Right;
        found = true;
      }
    }
  }

  return found;
}

}  // namespace dominoes
