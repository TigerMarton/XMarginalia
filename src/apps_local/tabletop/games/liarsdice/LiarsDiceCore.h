#pragma once

#include <cstdint>
#include <cstddef>

namespace liarsdice {

constexpr uint8_t kMaxPlayers = 4;
constexpr uint8_t kStartingDice = 5;

struct Bid {
  uint8_t count = 0;   // e.g. 4
  uint8_t face = 0;    // 1..6 (1 is wild / Ace)
  uint8_t bidder = 0;  // player index
};

enum class Phase : uint8_t {
  Bidding = 0,
  Showdown,
  RoundOver,
  GameOver
};

struct Player {
  uint8_t diceCount = kStartingDice;
  uint8_t dice[kStartingDice] = {1, 1, 1, 1, 1};
  bool isAi = true;
};

struct Game {
  uint8_t playerCount = 2;  // 2..4
  Player players[kMaxPlayers];
  Bid currentBid{};
  uint8_t activePlayer = 0;
  Phase phase = Phase::Bidding;

  // Showdown result data
  uint8_t challenger = 0;
  uint8_t roundLoser = 0;
  uint8_t totalMatchingDice = 0;
  bool challengeSuccessful = false;
  uint8_t winner = 0;
};

void initGame(Game& game, uint8_t numPlayers = 2, uint32_t seed = 0);
void rollPlayerDice(Game& game, uint8_t playerIdx, uint32_t seed = 0);
void startNewRound(Game& game, uint32_t seed = 0);

bool isValidBid(const Game& game, const Bid& nextBid);
bool makeBid(Game& game, const Bid& nextBid);
bool challenge(Game& game);

uint8_t countTotalMatching(const Game& game, uint8_t face);
uint8_t countTotalActiveDice(const Game& game);

uint8_t getNextActivePlayer(const Game& game, uint8_t current);

}  // namespace liarsdice
