#pragma once

#include <cstdint>
#include <cstddef>
#include "../../cards/Cards.h"

namespace sandbox {

enum class ToolMode : uint8_t {
  Dice = 0,
  Cards,
  Counters,
  Board,
};

enum class DieType : uint8_t {
  D4 = 4,
  D6 = 6,
  D8 = 8,
  D10 = 10,
  D12 = 12,
  D20 = 20,
};

constexpr int kMaxDice = 6;
constexpr int kMaxHand = 5;
constexpr int kMaxPlayers = 4;
constexpr int kMaxTokens = 32;

struct Token {
  uint8_t pos;    // 0..63 (rank * 8 + file)
  uint8_t color;  // 0 = White, 1 = Black
};

struct SandboxState {
  ToolMode mode;
  uint32_t rng;

  // Dice
  uint8_t diceCount;
  DieType dieType;
  uint8_t diceValues[kMaxDice];
  uint8_t heldMask;  // bit i set if die i held
  int16_t lastRollSum;
  int16_t rollHistory[3];

  // Cards
  uint8_t deck[cards::kDeck];  // 52 cards
  uint8_t deckCount;
  uint8_t hand[kMaxHand];
  uint8_t handCount;
  uint8_t discardTop;          // cards::kNoCard if empty
  bool handHidden;             // Secret hand privacy shroud

  // Counters
  int16_t scores[kMaxPlayers];
  uint8_t activePlayer;

  // Board
  Token tokens[kMaxTokens];
  uint8_t tokenCount;
  int8_t selectedToken;        // index into tokens or -1
  uint8_t spawnColor;          // 0 = White, 1 = Black
};

// Initialization
SandboxState makeDefault(uint32_t seed = 0x12345678);

// Dice API
void rollDice(SandboxState& state);
void toggleHoldDie(SandboxState& state, int index);
void addDie(SandboxState& state);
void removeDie(SandboxState& state);
void cycleDieType(SandboxState& state);

// Cards API
void initDeck(SandboxState& state);
void shuffleDeck(SandboxState& state);
bool drawCard(SandboxState& state);
bool discardCard(SandboxState& state, int handIndex);
void discardHand(SandboxState& state);
void flipHandCard(SandboxState& state, int handIndex);
void toggleHandHidden(SandboxState& state);

// Counters API
void changeScore(SandboxState& state, int player, int delta);
void resetScores(SandboxState& state, int initialValue);
void nextPlayer(SandboxState& state);

// Board API
int findTokenAt(const SandboxState& state, uint8_t pos);
bool spawnToken(SandboxState& state, uint8_t pos, uint8_t color);
bool moveToken(SandboxState& state, int tokenIndex, uint8_t newPos);
bool removeToken(SandboxState& state, int tokenIndex);
void clearTokens(SandboxState& state);
void setupCheckersPreset(SandboxState& state);
void toggleSpawnColor(SandboxState& state);

}  // namespace sandbox
