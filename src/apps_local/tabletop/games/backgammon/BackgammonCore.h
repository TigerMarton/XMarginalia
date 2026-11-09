#pragma once

#include <cstdint>
#include <cstddef>

namespace backgammon {

constexpr int kPoints = 24;
constexpr int kCheckersPerPlayer = 15;

constexpr int8_t kFromBar = -1;
constexpr int8_t kToOff = -2;

enum class Player : uint8_t {
  White = 0,  // South (moves 23 -> 0, bears off below 0)
  Black = 1,  // North (moves 0 -> 23, bears off above 23)
};

inline Player opponent(Player p) {
  return (p == Player::White) ? Player::Black : Player::White;
}

struct Move {
  int8_t from = 0;  // 0..23, or kFromBar (-1)
  int8_t to = 0;    // 0..23, or kToOff (-2)
  uint8_t die = 0;  // 1..6

  bool isBar() const { return from == kFromBar; }
  bool isOff() const { return to == kToOff; }
  bool operator==(const Move& o) const {
    return from == o.from && to == o.to && die == o.die;
  }
};

struct Game {
  // board[p] > 0: count of White checkers
  // board[p] < 0: -count of Black checkers
  // board[p] == 0: empty point
  int8_t board[kPoints] = {};

  uint8_t bar[2] = {};       // [0]=White, [1]=Black on bar
  uint8_t borneOff[2] = {};  // [0]=White, [1]=Black borne off

  uint8_t dice[2] = {};
  uint8_t movesLeft[4] = {};
  uint8_t moveCount = 0;

  Player turn = Player::White;
  bool isRolled = false;
  bool isOver = false;
  Player winner = Player::White;
  uint8_t winMultiplier = 1;  // 1=Single, 2=Gammon, 3=Backgammon
};

void initGame(Game& game);
void rollDice(Game& game, uint32_t seed);
bool canBearOff(const Game& game, Player p);
bool isLegalMove(const Game& game, const Move& move);
bool applyMove(Game& game, const Move& move);
size_t generateLegalMoves(const Game& game, Move* outMoves, size_t maxMoves);
int calculatePipCount(const Game& game, Player p);
void passTurn(Game& game);

}  // namespace backgammon
