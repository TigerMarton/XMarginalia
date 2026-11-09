#include "SantoriniBrain.h"

namespace santorini {

namespace {

constexpr int kInfinity = 1000000;

int centerDist(int cell) {
  int x = cellX(cell);
  int y = cellY(cell);
  int dx = (x > 2) ? (x - 2) : (2 - x);
  int dy = (y > 2) ? (y - 2) : (2 - y);
  return dx + dy;
}

int minimax(const State& state, int depth, int alpha, int beta, bool maximizing, Player aiPlayer) {
  if (state.phase == Phase::GameOver) {
    if (state.winner == aiPlayer) {
      return 100000 + depth * 100;
    } else {
      return -100000 - depth * 100;
    }
  }

  if (depth <= 0) {
    return SantoriniBrain::evaluate(state, aiPlayer);
  }

  Player curPlayer = state.turn;
  bool isAiTurn = (curPlayer == aiPlayer);

  int bestVal = isAiTurn ? -kInfinity : kInfinity;

  for (int w = 0; w < 2; ++w) {
    int8_t moves[8];
    int mCount = getLegalMovesForWorker(state, w, moves, 8);

    for (int m = 0; m < mCount; ++m) {
      State nextState = state;
      selectWorker(nextState, w);
      executeMove(nextState, moves[m]);

      if (nextState.phase == Phase::GameOver) {
        // Instant win by climbing to Level 3
        int val = (nextState.winner == aiPlayer) ? (100000 + depth * 100) : (-100000 - depth * 100);
        if (isAiTurn) {
          if (val > bestVal) bestVal = val;
          if (bestVal > alpha) alpha = bestVal;
        } else {
          if (val < bestVal) bestVal = val;
          if (bestVal < beta) beta = bestVal;
        }
        if (beta <= alpha) return bestVal;
        continue;
      }

      int8_t builds[8];
      int bCount = getLegalBuilds(nextState, builds, 8);

      for (int b = 0; b < bCount; ++b) {
        State nextState2 = nextState;
        executeBuild(nextState2, builds[b]);

        int val = minimax(nextState2, depth - 1, alpha, beta, !maximizing, aiPlayer);

        if (isAiTurn) {
          if (val > bestVal) bestVal = val;
          if (bestVal > alpha) alpha = bestVal;
        } else {
          if (val < bestVal) bestVal = val;
          if (bestVal < beta) beta = bestVal;
        }

        if (beta <= alpha) {
          return bestVal;  // Alpha-Beta cutoff
        }
      }
    }
  }

  return bestVal;
}

}  // namespace

int SantoriniBrain::evaluate(const State& state, Player aiPlayer) {
  if (state.phase == Phase::GameOver) {
    return (state.winner == aiPlayer) ? 100000 : -100000;
  }

  Player oppPlayer = opponent(aiPlayer);
  int score = 0;

  // 1. Worker heights
  for (int w = 0; w < 2; ++w) {
    int aiCell = state.workers[static_cast<int>(aiPlayer)][w];
    int aiHeight = state.board[aiCell];
    score += aiHeight * 50;
    score -= centerDist(aiCell) * 6;

    int oppCell = state.workers[static_cast<int>(oppPlayer)][w];
    int oppHeight = state.board[oppCell];
    score -= oppHeight * 60;
    score += centerDist(oppCell) * 6;
  }

  // 2. Threats: worker on Level 2 with adjacent un-domed Level 3
  for (int p = 0; p < 2; ++p) {
    Player curP = static_cast<Player>(p);
    bool isAi = (curP == aiPlayer);
    for (int w = 0; w < 2; ++w) {
      int cell = state.workers[p][w];
      if (state.board[cell] == 2) {
        int fx = cellX(cell);
        int fy = cellY(cell);
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = fx + dx;
            int ny = fy + dy;
            if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
              int adj = cellIdx(nx, ny);
              if (state.board[adj] == 3) {
                // Adjacent level 3 space!
                if (isAi) {
                  score += 400;
                } else {
                  score -= 600;  // High urgency to block
                }
              }
            }
          }
        }
      }
    }
  }

  return score;
}

bool SantoriniBrain::findBestMove(const State& state, Move& outMove, int depth) {
  if (state.phase == Phase::GameOver) return false;

  Player aiPlayer = state.turn;
  int bestVal = -kInfinity;
  bool found = false;

  int alpha = -kInfinity;
  int beta = kInfinity;

  for (int w = 0; w < 2; ++w) {
    int8_t moves[8];
    int mCount = getLegalMovesForWorker(state, w, moves, 8);

    for (int m = 0; m < mCount; ++m) {
      State nextState = state;
      selectWorker(nextState, w);
      executeMove(nextState, moves[m]);

      if (nextState.phase == Phase::GameOver && nextState.winner == aiPlayer) {
        // Immediate winning move! Take it immediately
        outMove.worker = static_cast<int8_t>(w);
        outMove.moveCell = moves[m];
        outMove.buildCell = moves[m];  // Build ignored on win
        return true;
      }

      int8_t builds[8];
      int bCount = getLegalBuilds(nextState, builds, 8);

      for (int b = 0; b < bCount; ++b) {
        State nextState2 = nextState;
        executeBuild(nextState2, builds[b]);

        int val = minimax(nextState2, depth - 1, alpha, beta, false, aiPlayer);

        if (!found || val > bestVal) {
          bestVal = val;
          outMove.worker = static_cast<int8_t>(w);
          outMove.moveCell = moves[m];
          outMove.buildCell = builds[b];
          found = true;
        }

        if (bestVal > alpha) {
          alpha = bestVal;
        }
      }
    }
  }

  return found;
}

}  // namespace santorini
