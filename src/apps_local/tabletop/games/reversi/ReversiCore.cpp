#include "ReversiCore.h"

namespace reversi {

namespace {

constexpr int kDirections[8][2] = {
    {0, -1},   // N
    {0, 1},    // S
    {-1, 0},   // W
    {1, 0},    // E
    {-1, -1},  // NW
    {1, -1},   // NE
    {-1, 1},   // SW
    {1, 1},    // SE
};

}  // namespace

void start(Game& game) {
  // Standard Othello start: D4=White (27), E4=Black (28), D5=Black (35), E5=White (36)
  game.black = (1ULL << 28) | (1ULL << 35);
  game.white = (1ULL << 27) | (1ULL << 36);
  game.turn = kBlack;
  game.isOver = false;
}

uint64_t getFlips(const Game& game, const int col, const int row, const uint8_t player) {
  if (col < 0 || col >= kSize || row < 0 || row >= kSize) return 0;
  const uint64_t targetMask = 1ULL << (row * kSize + col);
  if ((game.black | game.white) & targetMask) return 0;

  const uint64_t friendly = (player == kBlack) ? game.black : game.white;
  const uint64_t enemy = (player == kBlack) ? game.white : game.black;

  uint64_t totalFlips = 0;

  for (const auto& dir : kDirections) {
    int c = col + dir[0];
    int r = row + dir[1];
    uint64_t rayFlips = 0;

    while (c >= 0 && c < kSize && r >= 0 && r < kSize) {
      const uint64_t mask = 1ULL << (r * kSize + c);
      if (enemy & mask) {
        rayFlips |= mask;
      } else if (friendly & mask) {
        totalFlips |= rayFlips;
        break;
      } else {
        break;
      }
      c += dir[0];
      r += dir[1];
    }
  }

  return totalFlips;
}

uint64_t legalMoves(const Game& game, const uint8_t player) {
  if (game.isOver) return 0;
  uint64_t moves = 0;
  const uint64_t occupied = game.black | game.white;

  for (int r = 0; r < kSize; ++r) {
    for (int c = 0; c < kSize; ++c) {
      const uint64_t mask = 1ULL << (r * kSize + c);
      if (!(occupied & mask)) {
        if (getFlips(game, c, r, player) != 0) {
          moves |= mask;
        }
      }
    }
  }

  return moves;
}

bool hasLegalMoves(const Game& game, const uint8_t player) {
  return legalMoves(game, player) != 0;
}

bool playMove(Game& game, const int col, const int row) {
  if (game.isOver) return false;
  const uint64_t flips = getFlips(game, col, row, game.turn);
  if (flips == 0) return false;

  const uint64_t moveMask = 1ULL << (row * kSize + col);
  if (game.turn == kBlack) {
    game.black |= (moveMask | flips);
    game.white &= ~flips;
  } else {
    game.white |= (moveMask | flips);
    game.black &= ~flips;
  }

  const uint8_t next = opponent(game.turn);
  if (hasLegalMoves(game, next)) {
    game.turn = next;
  } else if (hasLegalMoves(game, game.turn)) {
    // Next player is blocked; turn passes back to current player
  } else {
    game.isOver = true;
  }

  return true;
}

bool passTurn(Game& game) {
  if (game.isOver) return false;
  if (hasLegalMoves(game, game.turn)) return false;  // Can only pass if no moves

  const uint8_t next = opponent(game.turn);
  if (hasLegalMoves(game, next)) {
    game.turn = next;
    return true;
  }

  game.isOver = true;
  return true;
}

Outcome outcome(const Game& game) {
  if (!game.isOver) return Outcome::InProgress;
  const int b = blackCount(game);
  const int w = whiteCount(game);
  if (b > w) return Outcome::BlackWon;
  if (w > b) return Outcome::WhiteWon;
  return Outcome::Draw;
}

}  // namespace reversi
