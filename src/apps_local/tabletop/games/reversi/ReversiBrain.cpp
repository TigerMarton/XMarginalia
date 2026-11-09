#include "ReversiBrain.h"

#include <algorithm>

namespace reversi {

namespace {

// Classic 8x8 positional weight matrix
constexpr int kWeights[64] = {
    100, -20,  10,   5,   5,  10, -20, 100,
    -20, -50,  -2,  -2,  -2,  -2, -50, -20,
     10,  -2,  -1,  -1,  -1,  -1,  -2,  10,
      5,  -2,  -1,   0,   0,  -1,  -2,   5,
      5,  -2,  -1,   0,   0,  -1,  -2,   5,
     10,  -2,  -1,  -1,  -1,  -1,  -2,  10,
    -20, -50,  -2,  -2,  -2,  -2, -50, -20,
    100, -20,  10,   5,   5,  10, -20, 100,
};

int evaluatePosition(const Game& game, const uint8_t player) {
  if (game.isOver) {
    const int b = blackCount(game);
    const int w = whiteCount(game);
    if (b == w) return 0;
    if (player == kBlack) {
      return b > w ? 10000 + (b - w) : -10000 - (w - b);
    } else {
      return w > b ? 10000 + (w - b) : -10000 - (b - w);
    }
  }

  int positional = 0;
  const uint64_t friendly = (player == kBlack) ? game.black : game.white;
  const uint64_t enemy = (player == kBlack) ? game.white : game.black;

  for (int i = 0; i < 64; ++i) {
    const uint64_t mask = 1ULL << i;
    if (friendly & mask) positional += kWeights[i];
    else if (enemy & mask) positional -= kWeights[i];
  }

  const int myMobility = countDiscs(legalMoves(game, player));
  const int oppMobility = countDiscs(legalMoves(game, opponent(player)));
  const int mobility = (myMobility - oppMobility) * 10;

  return positional + mobility;
}

int minimax(Game game, int depth, int alpha, int beta, bool maximizing, uint8_t aiPlayer) {
  if (depth == 0 || game.isOver) {
    return evaluatePosition(game, aiPlayer);
  }

  const uint8_t curPlayer = game.turn;
  const uint64_t moves = legalMoves(game, curPlayer);

  if (moves == 0) {
    // Pass turn
    Game nextGame = game;
    const uint8_t opp = opponent(curPlayer);
    if (hasLegalMoves(nextGame, opp)) {
      nextGame.turn = opp;
      return minimax(nextGame, depth - 1, alpha, beta, !maximizing, aiPlayer);
    } else {
      nextGame.isOver = true;
      return evaluatePosition(nextGame, aiPlayer);
    }
  }

  if (maximizing) {
    int maxEval = -100000;
    for (int r = 0; r < kSize; ++r) {
      for (int c = 0; c < kSize; ++c) {
        if (moves & (1ULL << (r * kSize + c))) {
          Game nextGame = game;
          playMove(nextGame, c, r);
          const int eval = minimax(nextGame, depth - 1, alpha, beta, false, aiPlayer);
          maxEval = std::max(maxEval, eval);
          alpha = std::max(alpha, eval);
          if (beta <= alpha) break;
        }
      }
    }
    return maxEval;
  } else {
    int minEval = 100000;
    for (int r = 0; r < kSize; ++r) {
      for (int c = 0; c < kSize; ++c) {
        if (moves & (1ULL << (r * kSize + c))) {
          Game nextGame = game;
          playMove(nextGame, c, r);
          const int eval = minimax(nextGame, depth - 1, alpha, beta, true, aiPlayer);
          minEval = std::min(minEval, eval);
          beta = std::min(beta, eval);
          if (beta <= alpha) break;
        }
      }
    }
    return minEval;
  }
}

}  // namespace

BestMove findBestMove(const Game& game, const uint8_t player, const Difficulty diff) {
  BestMove best;
  const uint64_t moves = legalMoves(game, player);
  if (moves == 0) return best;

  int maxDepth = 4;
  if (diff == Difficulty::Casual) maxDepth = 1;
  else if (diff == Difficulty::Master) maxDepth = 6;

  int bestScore = -100000;

  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      if (moves & (1ULL << (r * kSize + c))) {
        Game nextGame = game;
        playMove(nextGame, c, r);
        const int score = minimax(nextGame, maxDepth - 1, -100000, 100000, false, player);
        if (best.col == -1 || score > bestScore) {
          bestScore = score;
          best.col = c;
          best.row = r;
          best.score = score;
        }
      }
    }
  }

  return best;
}

}  // namespace reversi
