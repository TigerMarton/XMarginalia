#pragma once

#include <cstdint>

namespace mancala {

constexpr int kPitsTotal = 14;
constexpr int kPitsPerSide = 6;
constexpr int kSeedsPerPit = 4;

constexpr uint8_t kPlayer1 = 0;  // South (bottom row, pits 0..5, store 6)
constexpr uint8_t kPlayer2 = 1;  // North (top row, pits 7..12, store 13)

constexpr int kStore1 = 6;
constexpr int kStore2 = 13;

inline uint8_t opponent(const uint8_t player) {
  return player == kPlayer1 ? kPlayer2 : kPlayer1;
}

inline int storeOf(const uint8_t player) {
  return player == kPlayer1 ? kStore1 : kStore2;
}

inline int opponentStore(const uint8_t player) {
  return player == kPlayer1 ? kStore2 : kStore1;
}

inline bool isFriendlyPit(const uint8_t player, const int pit) {
  if (player == kPlayer1) {
    return pit >= 0 && pit < 6;
  } else {
    return pit >= 7 && pit < 13;
  }
}

inline int oppositePit(const int pit) {
  if (pit >= 0 && pit < 6) return 12 - pit;
  if (pit >= 7 && pit < 13) return 12 - pit;
  return -1;
}

struct Game {
  uint8_t pits[kPitsTotal] = {};
  uint8_t turn = kPlayer1;
  bool isOver = false;
  bool extraTurn = false;
  int lastMove = -1;
};

void start(Game& game);
int playerPitSeeds(const Game& game, uint8_t player);
bool hasLegalMoves(const Game& game, uint8_t player);
bool isLegalMove(const Game& game, uint8_t player, int pit);
bool playMove(Game& game, int pit);

enum class Outcome : uint8_t {
  InProgress,
  Player1Won,
  Player2Won,
  Draw,
};

Outcome outcome(const Game& game);

}  // namespace mancala
