#include "MorrisBrain.h"

#include <algorithm>

namespace morris {

namespace {

int countMills(const uint32_t pieces) {
  int count = 0;
  for (const uint32_t mill : kMills) {
    if ((pieces & mill) == mill) count++;
  }
  return count;
}

int calculateMobility(const Game& game, const uint8_t player) {
  const Phase phase = playerPhase(game, player);
  if (phase == Phase::Placing || phase == Phase::Flying) return 24;
  const uint32_t occupied = game.pieces[kWhite] | game.pieces[kBlack];
  int mobility = 0;
  const uint32_t myPieces = game.pieces[player];
  for (int v = 0; v < kVertices; ++v) {
    if (myPieces & (1U << v)) {
      mobility += __builtin_popcount(kAdjacentMask[v] & ~occupied);
    }
  }
  return mobility;
}

int evaluatePosition(const Game& game, const uint8_t aiPlayer) {
  const uint8_t opp = opponent(aiPlayer);
  if (game.isOver) {
    // If it's AI's turn when game is over, AI lost
    if (game.turn == aiPlayer) return -10000;
    return 10000;
  }

  const int myPieces = pieceCount(game.pieces[aiPlayer]);
  const int oppPieces = pieceCount(game.pieces[opp]);
  const int pieceScore = (myPieces - oppPieces) * 200;

  const int myMills = countMills(game.pieces[aiPlayer]);
  const int oppMills = countMills(game.pieces[opp]);
  const int millScore = (myMills - oppMills) * 150;

  const int myMob = calculateMobility(game, aiPlayer);
  const int oppMob = calculateMobility(game, opp);
  const int mobScore = (myMob - oppMob) * 15;

  return pieceScore + millScore + mobScore;
}

int minimax(Game game, int depth, int alpha, int beta, bool maximizing, uint8_t aiPlayer) {
  if (depth == 0 || game.isOver) {
    return evaluatePosition(game, aiPlayer);
  }

  const uint8_t curPlayer = game.turn;

  if (game.pendingRemoval) {
    const uint32_t removables = removablePieces(game, opponent(curPlayer));
    if (removables == 0) return evaluatePosition(game, aiPlayer);

    int bestVal = maximizing ? -100000 : 100000;
    for (int v = 0; v < kVertices; ++v) {
      if (removables & (1U << v)) {
        Game nextGame = game;
        removePiece(nextGame, v);
        const bool nextMax = (nextGame.turn == aiPlayer);
        const int val = minimax(nextGame, depth - 1, alpha, beta, nextMax, aiPlayer);
        if (maximizing) {
          bestVal = std::max(bestVal, val);
          alpha = std::max(alpha, val);
        } else {
          bestVal = std::min(bestVal, val);
          beta = std::min(beta, val);
        }
        if (beta <= alpha) break;
      }
    }
    return bestVal;
  }

  const Phase phase = playerPhase(game, curPlayer);

  if (phase == Phase::Placing) {
    const uint32_t placements = legalPlacements(game);
    if (placements == 0) return evaluatePosition(game, aiPlayer);

    int bestVal = maximizing ? -100000 : 100000;
    for (int v = 0; v < kVertices; ++v) {
      if (placements & (1U << v)) {
        Game nextGame = game;
        placePiece(nextGame, v);
        const bool nextMax = (nextGame.turn == aiPlayer);
        const int val = minimax(nextGame, depth - 1, alpha, beta, nextMax, aiPlayer);
        if (maximizing) {
          bestVal = std::max(bestVal, val);
          alpha = std::max(alpha, val);
        } else {
          bestVal = std::min(bestVal, val);
          beta = std::min(beta, val);
        }
        if (beta <= alpha) break;
      }
    }
    return bestVal;
  }

  // Moving or Flying
  int bestVal = maximizing ? -100000 : 100000;
  bool movedAny = false;
  const uint32_t myPieces = game.pieces[curPlayer];
  for (int from = 0; from < kVertices; ++from) {
    if (myPieces & (1U << from)) {
      const uint32_t dests = legalDestinations(game, from);
      for (int to = 0; to < kVertices; ++to) {
        if (dests & (1U << to)) {
          movedAny = true;
          Game nextGame = game;
          movePiece(nextGame, from, to);
          const bool nextMax = (nextGame.turn == aiPlayer);
          const int val = minimax(nextGame, depth - 1, alpha, beta, nextMax, aiPlayer);
          if (maximizing) {
            bestVal = std::max(bestVal, val);
            alpha = std::max(alpha, val);
          } else {
            bestVal = std::min(bestVal, val);
            beta = std::min(beta, val);
          }
          if (beta <= alpha) break;
        }
      }
      if (beta <= alpha) break;
    }
  }

  return movedAny ? bestVal : evaluatePosition(game, aiPlayer);
}

}  // namespace

BestMove findBestMove(const Game& game, const uint8_t player, const Difficulty diff) {
  BestMove best;
  if (game.isOver) return best;

  int maxDepth = 3;
  if (diff == Difficulty::Casual) maxDepth = 1;
  else if (diff == Difficulty::Master) maxDepth = 5;

  int bestScore = -100000;

  if (game.pendingRemoval) {
    best.kind = BestMove::Remove;
    const uint32_t removables = removablePieces(game, opponent(player));
    for (int v = 0; v < kVertices; ++v) {
      if (removables & (1U << v)) {
        Game nextGame = game;
        removePiece(nextGame, v);
        const bool nextMax = (nextGame.turn == player);
        const int score = minimax(nextGame, maxDepth - 1, -100000, 100000, nextMax, player);
        if (best.to == -1 || score > bestScore) {
          bestScore = score;
          best.to = static_cast<int8_t>(v);
          best.score = score;
        }
      }
    }
    return best;
  }

  const Phase phase = playerPhase(game, player);
  if (phase == Phase::Placing) {
    best.kind = BestMove::Place;
    const uint32_t placements = legalPlacements(game);
    for (int v = 0; v < kVertices; ++v) {
      if (placements & (1U << v)) {
        Game nextGame = game;
        placePiece(nextGame, v);
        const bool nextMax = (nextGame.turn == player);
        const int score = minimax(nextGame, maxDepth - 1, -100000, 100000, nextMax, player);
        if (best.to == -1 || score > bestScore) {
          bestScore = score;
          best.to = static_cast<int8_t>(v);
          best.score = score;
        }
      }
    }
    return best;
  }

  // Moving or Flying
  best.kind = BestMove::Slide;
  const uint32_t myPieces = game.pieces[player];
  for (int from = 0; from < kVertices; ++from) {
    if (myPieces & (1U << from)) {
      const uint32_t dests = legalDestinations(game, from);
      for (int to = 0; to < kVertices; ++to) {
        if (dests & (1U << to)) {
          Game nextGame = game;
          movePiece(nextGame, from, to);
          const bool nextMax = (nextGame.turn == player);
          const int score = minimax(nextGame, maxDepth - 1, -100000, 100000, nextMax, player);
          if (best.from == -1 || score > bestScore) {
            bestScore = score;
            best.from = static_cast<int8_t>(from);
            best.to = static_cast<int8_t>(to);
            best.score = score;
          }
        }
      }
    }
  }

  return best;
}

}  // namespace morris
