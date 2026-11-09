#include "BackgammonBrain.h"

namespace backgammon {

namespace {

constexpr int32_t kWinScore = 100000;
constexpr int32_t kInf = 1000000;

inline uint32_t xorshift32(uint32_t& state) {
  if (state == 0) state = 0x98765432;
  uint32_t x = state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

int32_t evaluateState(const Game& game, Player pov) {
  if (game.isOver) {
    if (game.winner == pov) return kWinScore * game.winMultiplier;
    return -kWinScore * game.winMultiplier;
  }

  const int myIdx = static_cast<int>(pov);
  const int oppIdx = 1 - myIdx;

  int32_t score = 0;

  // 1. Pip count advantage
  const int myPips = calculatePipCount(game, pov);
  const int oppPips = calculatePipCount(game, opponent(pov));
  score += (oppPips - myPips) * 5;

  // 2. Borne off advantage
  score += static_cast<int32_t>(game.borneOff[myIdx]) * 120;
  score -= static_cast<int32_t>(game.borneOff[oppIdx]) * 120;

  // 3. Bar penalty
  score -= static_cast<int32_t>(game.bar[myIdx]) * 150;
  score += static_cast<int32_t>(game.bar[oppIdx]) * 150;

  // 4. Board points evaluation: blots vs blocks
  for (int pt = 0; pt < kPoints; ++pt) {
    const int count = game.board[pt];
    if (count == 0) continue;

    if (pov == Player::White) {
      if (count == 1) {
        // Exposed blot
        score -= 40;
      } else if (count > 1) {
        // Safe anchor / made point
        score += 20;
        if (pt <= 5) score += 25;  // Home board prime
      }

      if (count == -1) {
        // Opponent blot vulnerable
        score += 30;
      }
    } else {
      if (count == -1) {
        // Exposed blot
        score -= 40;
      } else if (count < -1) {
        // Safe anchor / made point
        score += 20;
        if (pt >= 18) score += 25;  // Home board prime
      }

      if (count == 1) {
        // Opponent blot vulnerable
        score += 30;
      }
    }
  }

  return score;
}

}  // namespace

Move chooseBotMove(const Game& game, Difficulty diff, uint32_t seed) {
  Move moves[64];
  const size_t nMoves = generateLegalMoves(game, moves, 64);
  if (nMoves == 0) return Move{};

  const Player botPlayer = game.turn;

  Move bestMove = moves[0];
  int32_t bestScore = -kInf;

  for (size_t i = 0; i < nMoves; ++i) {
    Game clone = game;
    applyMove(clone, moves[i]);

    int32_t score = evaluateState(clone, botPlayer);

    if (diff == Difficulty::Casual) {
      const int32_t jitter = static_cast<int32_t>((xorshift32(seed) % 80) - 40);
      score += jitter;
    }

    if (score > bestScore) {
      bestScore = score;
      bestMove = moves[i];
    }
  }

  return bestMove;
}

}  // namespace backgammon
