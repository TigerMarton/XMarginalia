#include "QuoridorBrain.h"

namespace quoridor {

namespace {

constexpr int kInfinity = 1000000;

int minimax(const State& state, int depth, int alpha, int beta, bool maximizing, Player aiPlayer) {
  if (state.winner != Player::None) {
    if (state.winner == aiPlayer) {
      return 100000 + depth * 100;
    } else {
      return -100000 - depth * 100;
    }
  }

  if (depth <= 0) {
    return QuoridorBrain::evaluate(state, aiPlayer);
  }

  Player curP = state.turn;
  bool isAiTurn = (curP == aiPlayer);
  int bestVal = isAiTurn ? -kInfinity : kInfinity;

  // 1. Pawn moves
  int8_t pawnMoves[8];
  int pCount = getLegalPawnMoves(state, pawnMoves, 8);

  for (int i = 0; i < pCount; ++i) {
    State nextState = state;
    executePawnMove(nextState, pawnMoves[i]);

    int val = minimax(nextState, depth - 1, alpha, beta, !maximizing, aiPlayer);

    if (isAiTurn) {
      if (val > bestVal) bestVal = val;
      if (bestVal > alpha) alpha = bestVal;
    } else {
      if (val < bestVal) bestVal = val;
      if (bestVal < beta) beta = bestVal;
    }
    if (beta <= alpha) return bestVal;
  }

  // 2. Candidate walls (if player has walls left)
  int pIdx = static_cast<int>(curP);
  if (state.wallsLeft[pIdx] > 0) {
    // Collect walls that obstruct the opponent
    Player oppP = opponent(curP);
    int oppCell = state.pawn[static_cast<int>(oppP)];
    int ox = cellX(oppCell);
    int oy = cellY(oppCell);

    // Search around opponent's vicinity
    for (int gy = oy - 2; gy <= oy + 1; ++gy) {
      if (gy < 0 || gy >= kInterRows) continue;
      for (int gx = ox - 2; gx <= ox + 1; ++gx) {
        if (gx < 0 || gx >= kInterCols) continue;

        for (int o = 0; o < 2; ++o) {
          WallOrientation orient = static_cast<WallOrientation>(o);
          if (canPlaceWall(state, gx, gy, orient)) {
            State nextState = state;
            executeWallPlacement(nextState, gx, gy, orient);

            int val = minimax(nextState, depth - 1, alpha, beta, !maximizing, aiPlayer);

            if (isAiTurn) {
              if (val > bestVal) bestVal = val;
              if (bestVal > alpha) alpha = bestVal;
            } else {
              if (val < bestVal) bestVal = val;
              if (bestVal < beta) beta = bestVal;
            }
            if (beta <= alpha) return bestVal;
          }
        }
      }
    }
  }

  return bestVal;
}

}  // namespace

int QuoridorBrain::evaluate(const State& state, Player aiPlayer) {
  if (state.winner != Player::None) {
    return (state.winner == aiPlayer) ? 100000 : -100000;
  }

  Player oppPlayer = opponent(aiPlayer);

  int myDist = getShortestPathLength(state, aiPlayer);
  int oppDist = getShortestPathLength(state, oppPlayer);

  if (myDist == -1) myDist = 100;
  if (oppDist == -1) oppDist = 100;

  int score = (oppDist - myDist) * 100;

  int aiIdx = static_cast<int>(aiPlayer);
  int oppIdx = static_cast<int>(oppPlayer);
  score += (state.wallsLeft[aiIdx] - state.wallsLeft[oppIdx]) * 15;

  return score;
}

bool QuoridorBrain::findBestMove(const State& state, Move& outMove, int depth) {
  if (state.winner != Player::None) return false;

  Player aiPlayer = state.turn;
  int bestVal = -kInfinity;
  bool found = false;

  int alpha = -kInfinity;
  int beta = kInfinity;

  // 1. Evaluate Pawn Moves
  int8_t pawnMoves[8];
  int pCount = getLegalPawnMoves(state, pawnMoves, 8);

  for (int i = 0; i < pCount; ++i) {
    State nextState = state;
    executePawnMove(nextState, pawnMoves[i]);

    if (nextState.winner == aiPlayer) {
      // Immediate win!
      outMove.isWall = false;
      outMove.targetCell = pawnMoves[i];
      return true;
    }

    int val = minimax(nextState, depth - 1, alpha, beta, false, aiPlayer);

    if (!found || val > bestVal) {
      bestVal = val;
      outMove.isWall = false;
      outMove.targetCell = pawnMoves[i];
      found = true;
    }
    if (bestVal > alpha) alpha = bestVal;
  }

  // 2. Evaluate Candidate Walls (around opponent)
  int pIdx = static_cast<int>(aiPlayer);
  if (state.wallsLeft[pIdx] > 0) {
    Player oppP = opponent(aiPlayer);
    int oppCell = state.pawn[static_cast<int>(oppP)];
    int ox = cellX(oppCell);
    int oy = cellY(oppCell);

    for (int gy = oy - 2; gy <= oy + 1; ++gy) {
      if (gy < 0 || gy >= kInterRows) continue;
      for (int gx = ox - 2; gx <= ox + 1; ++gx) {
        if (gx < 0 || gx >= kInterCols) continue;

        for (int o = 0; o < 2; ++o) {
          WallOrientation orient = static_cast<WallOrientation>(o);
          if (canPlaceWall(state, gx, gy, orient)) {
            State nextState = state;
            executeWallPlacement(nextState, gx, gy, orient);

            int val = minimax(nextState, depth - 1, alpha, beta, false, aiPlayer);

            if (!found || val > bestVal) {
              bestVal = val;
              outMove.isWall = true;
              outMove.wallGx = static_cast<int8_t>(gx);
              outMove.wallGy = static_cast<int8_t>(gy);
              outMove.wallOrient = orient;
              found = true;
            }
            if (bestVal > alpha) alpha = bestVal;
          }
        }
      }
    }
  }

  return found;
}

}  // namespace quoridor
