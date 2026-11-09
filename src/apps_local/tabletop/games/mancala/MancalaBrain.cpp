#include "MancalaBrain.h"

#include <algorithm>

namespace mancala {

namespace {

int evaluatePosition(const Game& game, const uint8_t aiPlayer) {
  const uint8_t opp = opponent(aiPlayer);
  const int aiStore = game.pits[storeOf(aiPlayer)];
  const int oppStore = game.pits[storeOf(opp)];

  if (game.isOver) {
    if (aiStore > oppStore) return 10000 + (aiStore - oppStore);
    if (oppStore > aiStore) return -10000 - (oppStore - aiStore);
    return 0;
  }

  const int storeDiff = (aiStore - oppStore) * 100;
  const int aiSeeds = playerPitSeeds(game, aiPlayer);
  const int oppSeeds = playerPitSeeds(game, opp);
  const int pitDiff = (aiSeeds - oppSeeds) * 10;

  return storeDiff + pitDiff;
}

int minimax(Game game, int depth, int alpha, int beta, bool maximizing, uint8_t aiPlayer) {
  if (depth == 0 || game.isOver) {
    return evaluatePosition(game, aiPlayer);
  }

  const uint8_t curPlayer = game.turn;
  const int startPit = (curPlayer == kPlayer1) ? 0 : 7;

  if (maximizing) {
    int maxEval = -100000;
    bool hasMove = false;
    for (int i = 0; i < 6; ++i) {
      const int pit = startPit + i;
      if (game.pits[pit] > 0) {
        hasMove = true;
        Game nextGame = game;
        playMove(nextGame, pit);
        // If extra turn, next move is still maximizing
        const bool nextMax = (nextGame.turn == aiPlayer);
        const int eval = minimax(nextGame, depth - 1, alpha, beta, nextMax, aiPlayer);
        maxEval = std::max(maxEval, eval);
        alpha = std::max(alpha, eval);
        if (beta <= alpha) break;
      }
    }
    return hasMove ? maxEval : evaluatePosition(game, aiPlayer);
  } else {
    int minEval = 100000;
    bool hasMove = false;
    for (int i = 0; i < 6; ++i) {
      const int pit = startPit + i;
      if (game.pits[pit] > 0) {
        hasMove = true;
        Game nextGame = game;
        playMove(nextGame, pit);
        // If extra turn for opp, next move is still minimizing
        const bool nextMax = (nextGame.turn == aiPlayer);
        const int eval = minimax(nextGame, depth - 1, alpha, beta, nextMax, aiPlayer);
        minEval = std::min(minEval, eval);
        beta = std::min(beta, eval);
        if (beta <= alpha) break;
      }
    }
    return hasMove ? minEval : evaluatePosition(game, aiPlayer);
  }
}

}  // namespace

BestMove findBestMove(const Game& game, const uint8_t player, const Difficulty diff) {
  BestMove best;
  if (game.isOver) return best;

  int maxDepth = 6;
  if (diff == Difficulty::Casual) maxDepth = 2;
  else if (diff == Difficulty::Master) maxDepth = 10;

  const int startPit = (player == kPlayer1) ? 0 : 7;
  int bestScore = -100000;

  for (int i = 0; i < 6; ++i) {
    const int pit = startPit + i;
    if (game.pits[pit] > 0) {
      Game nextGame = game;
      playMove(nextGame, pit);
      const bool nextMax = (nextGame.turn == player);
      const int score = minimax(nextGame, maxDepth - 1, -100000, 100000, nextMax, player);
      if (best.pit == -1 || score > bestScore) {
        bestScore = score;
        best.pit = pit;
        best.score = score;
      }
    }
  }

  return best;
}

}  // namespace mancala
