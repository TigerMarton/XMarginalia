#pragma once

#include <cstddef>
#include <cstdint>

namespace dominoes {

constexpr int kTotalTiles = 28;
constexpr int kHandSize = 7;

struct Tile {
  int8_t pips[2];
};

constexpr Tile kTiles[kTotalTiles] = {
    {0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6},
    {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6},
    {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 6},
    {3, 3}, {3, 4}, {3, 5}, {3, 6},
    {4, 4}, {4, 5}, {4, 6},
    {5, 5}, {5, 6},
    {6, 6}
};

enum class Player : uint8_t {
  P1 = 0,
  P2 = 1,
  None = 2,
};

inline Player opponent(Player p) {
  return (p == Player::P1) ? Player::P2 : Player::P1;
}

enum class Mode : uint8_t {
  Solo = 0,
  TwoPlayer = 1,
  Nearby = 2,
};

enum class PlaySide : uint8_t {
  Left = 0,
  Right = 1,
};

struct PlayedTile {
  int8_t tileIdx;
  int8_t pips[2];  // pips[0] is Left, pips[1] is Right as placed on board
};

struct Move {
  int8_t handIdx;
  PlaySide side;
};

struct State {
  int8_t hand[2][kTotalTiles];
  uint8_t handCount[2];
  int8_t boneyard[kTotalTiles];
  uint8_t boneyardCount;

  PlayedTile train[kTotalTiles];
  uint8_t trainCount;
  int8_t leftEnd;
  int8_t rightEnd;

  Player turn;
  Player winner;
  Mode mode;
  bool privacyConcealed;
  int8_t selectedHandIdx;
  bool roundOver;
  int score[2];
  uint8_t consecutivePasses;
};

void initGame(State& state, uint32_t seed = 42);

bool canPlayLeft(const State& state, int8_t tileIdx);
bool canPlayRight(const State& state, int8_t tileIdx);
bool hasAnyLegalPlay(const State& state, Player p);

bool drawFromBoneyard(State& state);
bool passTurn(State& state);
bool playTile(State& state, int handIdx, PlaySide side);

int getHandPipSum(const State& state, Player p);

}  // namespace dominoes
