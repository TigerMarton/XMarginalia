#include "DobutsuBrain.h"

namespace dobutsu {

namespace {

constexpr int32_t kWinScore = 100000;
constexpr int32_t kInf = 1000000;
constexpr size_t kMaxLegalMoves = 48;

inline uint32_t xorshift32(uint32_t& state) {
  if (state == 0) state = 0x12345678;
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

int32_t pieceMaterialValue(PieceType type) {
  switch (type) {
    case PieceType::Lion: return 10000;
    case PieceType::Hen: return 500;
    case PieceType::Giraffe: return 350;
    case PieceType::Elephant: return 300;
    case PieceType::Chick: return 100;
    default: return 0;
  }
}

int32_t reserveMaterialValue(size_t rIdx) {
  switch (rIdx) {
    case 0: return 320;  // Giraffe
    case 1: return 280;  // Elephant
    case 2: return 90;   // Chick
    default: return 0;
  }
}

int32_t evaluateBoard(const Game& game, Player pov) {
  if (game.isOver) {
    if (game.winner == pov) return kWinScore;
    if (game.winner == opponent(pov)) return -kWinScore;
    return 0;
  }

  int32_t score = 0;

  // 1. Board pieces
  for (int8_t sq = 0; sq < kCells; ++sq) {
    const Cell c = game.board[sq];
    if (c == 0) continue;

    const Player owner = pieceOwner(c);
    const PieceType pt = pieceType(c);
    int32_t val = pieceMaterialValue(pt);

    const int8_t y = static_cast<int8_t>(sq / kCols);

    // Advanced Chick/Hen bonuses
    if (pt == PieceType::Chick) {
      if (owner == Player::P1 && y <= 1) val += 50;
      if (owner == Player::P2 && y >= 2) val += 50;
    }
    // Center control (cells 4, 7)
    if (sq == 4 || sq == 7) {
      val += 20;
    }

    if (owner == pov) {
      score += val;
    } else {
      score -= val;
    }
  }

  // 2. Reserve pieces
  const int povIdx = (pov == Player::P1) ? 0 : 1;
  const int oppIdx = 1 - povIdx;

  for (size_t r = 0; r < 3; ++r) {
    score += static_cast<int32_t>(game.reserves[povIdx][r]) * reserveMaterialValue(r);
    score -= static_cast<int32_t>(game.reserves[oppIdx][r]) * reserveMaterialValue(r);
  }

  return score;
}

int32_t minimax(Game& game, int depth, int32_t alpha, int32_t beta, Player pov) {
  if (depth <= 0 || game.isOver) {
    return evaluateBoard(game, pov);
  }

  Move moves[kMaxLegalMoves];
  const size_t nMoves = generateLegalMoves(game, moves, kMaxLegalMoves);
  if (nMoves == 0) {
    return evaluateBoard(game, pov);
  }

  const bool isMaximizing = (game.turn == pov);

  if (isMaximizing) {
    int32_t maxEval = -kInf;
    for (size_t i = 0; i < nMoves; ++i) {
      Game clone = game;
      applyMove(clone, moves[i]);
      const int32_t eval = minimax(clone, depth - 1, alpha, beta, pov);
      if (eval > maxEval) maxEval = eval;
      if (eval > alpha) alpha = eval;
      if (beta <= alpha) break;
    }
    return maxEval;
  } else {
    int32_t minEval = kInf;
    for (size_t i = 0; i < nMoves; ++i) {
      Game clone = game;
      applyMove(clone, moves[i]);
      const int32_t eval = minimax(clone, depth - 1, alpha, beta, pov);
      if (eval < minEval) minEval = eval;
      if (eval < beta) beta = eval;
      if (beta <= alpha) break;
    }
    return minEval;
  }
}

}  // namespace

Move chooseBotMove(const Game& game, Difficulty diff, uint32_t seed) {
  Move moves[kMaxLegalMoves];
  const size_t nMoves = generateLegalMoves(game, moves, kMaxLegalMoves);
  if (nMoves == 0) return Move{};

  int searchDepth = 3;
  if (diff == Difficulty::Casual) searchDepth = 2;
  else if (diff == Difficulty::Master) searchDepth = 4;

  const Player botPlayer = game.turn;

  Move bestMove = moves[0];
  int32_t bestScore = -kInf;

  for (size_t i = 0; i < nMoves; ++i) {
    Game clone = game;
    applyMove(clone, moves[i]);

    // Instant win check
    if (clone.isOver && clone.winner == botPlayer) {
      return moves[i];
    }

    int32_t score = minimax(clone, searchDepth - 1, -kInf, kInf, botPlayer);

    // Casual difficulty has small jitter to occasionally play sub-optimally
    if (diff == Difficulty::Casual) {
      const int32_t jitter = static_cast<int32_t>((xorshift32(seed) % 60) - 30);
      score += jitter;
    }

    if (score > bestScore) {
      bestScore = score;
      bestMove = moves[i];
    }
  }

  return bestMove;
}

}  // namespace dobutsu
