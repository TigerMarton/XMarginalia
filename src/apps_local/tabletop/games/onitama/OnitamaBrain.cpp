#include "OnitamaBrain.h"

#include <cstdlib>

namespace onitama {
namespace {

constexpr int kWinScore = 100000;

int evaluate(const Game& game, const uint8_t aiPlayer) {
  if (game.isOver) {
    if (game.winner == aiPlayer) return kWinScore;
    return -kWinScore;
  }

  int score = 0;
  int p1Students = 0;
  int p2Students = 0;
  int p1MasterIdx = -1;
  int p2MasterIdx = -1;

  for (int i = 0; i < kBoardCells; ++i) {
    const uint8_t p = game.cells[i];
    if (p == kP1Student) ++p1Students;
    else if (p == kP2Student) ++p2Students;
    else if (p == kP1Master) p1MasterIdx = i;
    else if (p == kP2Master) p2MasterIdx = i;
  }

  // Material evaluation
  int material = (p1Students - p2Students) * 200;

  // Temple proximity for Masters
  int p1TempleDist = 10;
  int p2TempleDist = 10;
  if (p1MasterIdx >= 0) {
    const int mx = p1MasterIdx % kBoardWidth;
    const int my = p1MasterIdx / kBoardWidth;
    p1TempleDist = std::abs(mx - 2) + std::abs(my - 0);
  }
  if (p2MasterIdx >= 0) {
    const int mx = p2MasterIdx % kBoardWidth;
    const int my = p2MasterIdx / kBoardWidth;
    p2TempleDist = std::abs(mx - 2) + std::abs(my - 4);
  }

  // Bonus for closer master to enemy temple
  material += (8 - p1TempleDist) * 40;
  material -= (8 - p2TempleDist) * 40;

  score = (aiPlayer == kPlayer1) ? material : -material;
  return score;
}

int minimax(Game& game, const int depth, int alpha, int beta, const bool maximizing, const uint8_t aiPlayer) {
  if (depth <= 0 || game.isOver) {
    const int ev = evaluate(game, aiPlayer);
    if (ev >= kWinScore / 2) return ev - (10 - depth) * 100;
    if (ev <= -kWinScore / 2) return ev + (10 - depth) * 100;
    return ev;
  }

  Move moves[64];
  const int numMoves = legalMoves(game, moves, 64);
  if (numMoves == 0) {
    // If no legal moves, turn simply passes
    Game next = game;
    next.turn = (next.turn == kPlayer1) ? kPlayer2 : kPlayer1;
    return -minimax(next, depth - 1, -beta, -alpha, !maximizing, aiPlayer);
  }

  if (maximizing) {
    int maxEval = -200000;
    for (int i = 0; i < numMoves; ++i) {
      Game next = game;
      applyMove(next, moves[i]);
      const int ev = minimax(next, depth - 1, alpha, beta, false, aiPlayer);
      if (ev > maxEval) maxEval = ev;
      if (ev > alpha) alpha = ev;
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    int minEval = 200000;
    for (int i = 0; i < numMoves; ++i) {
      Game next = game;
      applyMove(next, moves[i]);
      const int ev = minimax(next, depth - 1, alpha, beta, true, aiPlayer);
      if (ev < minEval) minEval = ev;
      if (ev < beta) beta = ev;
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

}  // namespace

Move chooseMove(const Game& game, const Difficulty diff) {
  Move best = {};
  if (game.isOver) return best;

  Move moves[64];
  const int count = legalMoves(game, moves, 64);
  if (count == 0) return best;
  best = moves[0];

  const int depth = static_cast<int>(diff);
  const uint8_t ai = game.turn;
  int bestScore = -300000;
  int alpha = -300000;
  constexpr int beta = 300000;

  for (int i = 0; i < count; ++i) {
    Game next = game;
    applyMove(next, moves[i]);
    const int score = minimax(next, depth - 1, alpha, beta, false, ai);
    if (score > bestScore) {
      bestScore = score;
      best = moves[i];
    }
    if (score > alpha) {
      alpha = score;
    }
  }

  return best;
}

}  // namespace onitama
