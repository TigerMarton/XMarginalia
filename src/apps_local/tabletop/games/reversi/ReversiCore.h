#pragma once

#include <cstdint>

namespace reversi {

constexpr int kSize = 8;
constexpr int kCells = 64;

constexpr uint8_t kEmpty = 0;
constexpr uint8_t kBlack = 1;
constexpr uint8_t kWhite = 2;

inline uint8_t opponent(const uint8_t player) {
  return player == kBlack ? kWhite : kBlack;
}

struct Game {
  uint64_t black = 0;
  uint64_t white = 0;
  uint8_t turn = kBlack;
  bool isOver = false;
};

void start(Game& game);
uint64_t legalMoves(const Game& game, uint8_t player);
bool hasLegalMoves(const Game& game, uint8_t player);
uint64_t getFlips(const Game& game, int col, int row, uint8_t player);
bool playMove(Game& game, int col, int row);
bool passTurn(Game& game);

inline uint8_t cellAt(const Game& game, const int col, const int row) {
  if (col < 0 || col >= kSize || row < 0 || row >= kSize) return kEmpty;
  const uint64_t mask = 1ULL << (row * kSize + col);
  if (game.black & mask) return kBlack;
  if (game.white & mask) return kWhite;
  return kEmpty;
}

inline int countDiscs(const uint64_t bitboard) {
  return static_cast<int>(__builtin_popcountll(bitboard));
}

inline int blackCount(const Game& game) { return countDiscs(game.black); }
inline int whiteCount(const Game& game) { return countDiscs(game.white); }

enum class Outcome : uint8_t {
  InProgress,
  BlackWon,
  WhiteWon,
  Draw,
};

Outcome outcome(const Game& game);

}  // namespace reversi
