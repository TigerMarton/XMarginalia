#include "SandboxCore.h"

#include <algorithm>
#include <cstring>

namespace sandbox {

namespace {

uint32_t nextRng(uint32_t& state) {
  uint32_t x = state;
  if (x == 0) x = 0x12345678;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

}  // namespace

SandboxState makeDefault(const uint32_t seed) {
  SandboxState s{};
  s.mode = ToolMode::Dice;
  s.rng = (seed != 0) ? seed : 0x12345678;

  // Dice default: 2d6
  s.diceCount = 2;
  s.dieType = DieType::D6;
  s.diceValues[0] = 3;
  s.diceValues[1] = 4;
  s.heldMask = 0;
  s.lastRollSum = 7;
  s.rollHistory[0] = 7;
  s.rollHistory[1] = 0;
  s.rollHistory[2] = 0;

  // Cards default: full 52-card deck, shuffled
  initDeck(s);

  // Counters default: 4 players at 20 points
  resetScores(s, 20);
  s.activePlayer = 0;

  // Board default: empty felt, White spawn
  s.tokenCount = 0;
  s.selectedToken = -1;
  s.spawnColor = 0;

  return s;
}

// ---------------------------------------------------------------------------
// Dice Implementation
// ---------------------------------------------------------------------------

void rollDice(SandboxState& state) {
  const uint8_t sides = static_cast<uint8_t>(state.dieType);
  int16_t sum = 0;
  for (int i = 0; i < state.diceCount && i < kMaxDice; ++i) {
    if ((state.heldMask & (1U << i)) == 0) {
      state.diceValues[i] = static_cast<uint8_t>(1 + (nextRng(state.rng) % sides));
    }
    sum = static_cast<int16_t>(sum + state.diceValues[i]);
  }
  state.lastRollSum = sum;
  state.rollHistory[2] = state.rollHistory[1];
  state.rollHistory[1] = state.rollHistory[0];
  state.rollHistory[0] = sum;
}

void toggleHoldDie(SandboxState& state, const int index) {
  if (index >= 0 && index < state.diceCount) {
    state.heldMask ^= (1U << index);
  }
}

void addDie(SandboxState& state) {
  if (state.diceCount < kMaxDice) {
    const uint8_t sides = static_cast<uint8_t>(state.dieType);
    state.diceValues[state.diceCount] = static_cast<uint8_t>(1 + (nextRng(state.rng) % sides));
    state.lastRollSum = static_cast<int16_t>(state.lastRollSum + state.diceValues[state.diceCount]);
    state.diceCount++;
  }
}

void removeDie(SandboxState& state) {
  if (state.diceCount > 1) {
    state.diceCount--;
    state.heldMask &= ~(1U << state.diceCount);
    // Recalculate sum
    int16_t sum = 0;
    for (int i = 0; i < state.diceCount; ++i) {
      sum = static_cast<int16_t>(sum + state.diceValues[i]);
    }
    state.lastRollSum = sum;
  }
}

void cycleDieType(SandboxState& state) {
  switch (state.dieType) {
    case DieType::D4:
      state.dieType = DieType::D6;
      break;
    case DieType::D6:
      state.dieType = DieType::D8;
      break;
    case DieType::D8:
      state.dieType = DieType::D10;
      break;
    case DieType::D10:
      state.dieType = DieType::D12;
      break;
    case DieType::D12:
      state.dieType = DieType::D20;
      break;
    case DieType::D20:
      state.dieType = DieType::D4;
      break;
  }
  state.heldMask = 0;
  rollDice(state);
}

// ---------------------------------------------------------------------------
// Cards Implementation
// ---------------------------------------------------------------------------

void initDeck(SandboxState& state) {
  int idx = 0;
  for (int s = 0; s < cards::kSuits; ++s) {
    for (int r = 0; r < cards::kRanks; ++r) {
      state.deck[idx++] = cards::makeCard(static_cast<cards::Suit>(s), r);
    }
  }
  state.deckCount = cards::kDeck;
  state.handCount = 0;
  state.discardTop = cards::kNoCard;
  state.handHidden = false;
  shuffleDeck(state);
}

void shuffleDeck(SandboxState& state) {
  // Fisher-Yates shuffle remaining cards
  for (int i = state.deckCount - 1; i > 0; --i) {
    const int j = static_cast<int>(nextRng(state.rng) % static_cast<uint32_t>(i + 1));
    const uint8_t tmp = state.deck[i];
    state.deck[i] = state.deck[j];
    state.deck[j] = tmp;
  }
}

bool drawCard(SandboxState& state) {
  if (state.deckCount == 0 || state.handCount >= kMaxHand) {
    return false;
  }
  const uint8_t card = state.deck[--state.deckCount];
  state.hand[state.handCount++] = cards::faceUp(card);
  return true;
}

bool discardCard(SandboxState& state, const int handIndex) {
  if (handIndex < 0 || handIndex >= state.handCount) {
    return false;
  }
  state.discardTop = cards::faceUp(state.hand[handIndex]);
  for (int i = handIndex; i < state.handCount - 1; ++i) {
    state.hand[i] = state.hand[i + 1];
  }
  state.handCount--;
  return true;
}

void discardHand(SandboxState& state) {
  if (state.handCount > 0) {
    state.discardTop = cards::faceUp(state.hand[state.handCount - 1]);
    state.handCount = 0;
  }
}

void flipHandCard(SandboxState& state, const int handIndex) {
  if (handIndex >= 0 && handIndex < state.handCount) {
    const uint8_t card = state.hand[handIndex];
    state.hand[handIndex] = cards::isFaceUp(card) ? cards::faceDown(card) : cards::faceUp(card);
  }
}

void toggleHandHidden(SandboxState& state) {
  state.handHidden = !state.handHidden;
}

// ---------------------------------------------------------------------------
// Counters Implementation
// ---------------------------------------------------------------------------

void changeScore(SandboxState& state, const int player, const int delta) {
  if (player >= 0 && player < kMaxPlayers) {
    int val = state.scores[player] + delta;
    if (val > 999) val = 999;
    if (val < -999) val = -999;
    state.scores[player] = static_cast<int16_t>(val);
  }
}

void resetScores(SandboxState& state, const int initialValue) {
  for (int i = 0; i < kMaxPlayers; ++i) {
    state.scores[i] = static_cast<int16_t>(initialValue);
  }
}

void nextPlayer(SandboxState& state) {
  state.activePlayer = static_cast<uint8_t>((state.activePlayer + 1) % kMaxPlayers);
}

// ---------------------------------------------------------------------------
// Board Implementation
// ---------------------------------------------------------------------------

int findTokenAt(const SandboxState& state, const uint8_t pos) {
  for (int i = 0; i < state.tokenCount; ++i) {
    if (state.tokens[i].pos == pos) return i;
  }
  return -1;
}

bool spawnToken(SandboxState& state, const uint8_t pos, const uint8_t color) {
  if (pos >= 64 || state.tokenCount >= kMaxTokens) return false;
  if (findTokenAt(state, pos) != -1) return false;
  state.tokens[state.tokenCount++] = Token{pos, color};
  return true;
}

bool moveToken(SandboxState& state, const int tokenIndex, const uint8_t newPos) {
  if (tokenIndex < 0 || tokenIndex >= state.tokenCount || newPos >= 64) return false;
  // If destination occupied by another token, capture/remove it
  const int target = findTokenAt(state, newPos);
  if (target != -1 && target != tokenIndex) {
    removeToken(state, target);
  }
  state.tokens[tokenIndex].pos = newPos;
  state.selectedToken = -1;
  return true;
}

bool removeToken(SandboxState& state, const int tokenIndex) {
  if (tokenIndex < 0 || tokenIndex >= state.tokenCount) return false;
  for (int i = tokenIndex; i < state.tokenCount - 1; ++i) {
    state.tokens[i] = state.tokens[i + 1];
  }
  state.tokenCount--;
  if (state.selectedToken == tokenIndex) {
    state.selectedToken = -1;
  } else if (state.selectedToken > tokenIndex) {
    state.selectedToken--;
  }
  return true;
}

void clearTokens(SandboxState& state) {
  state.tokenCount = 0;
  state.selectedToken = -1;
}

void setupCheckersPreset(SandboxState& state) {
  clearTokens(state);
  // White pieces on ranks 0..2 (dark squares)
  for (int r = 0; r < 3; ++r) {
    for (int c = 0; c < 8; ++c) {
      if ((r + c) % 2 == 1) {
        spawnToken(state, static_cast<uint8_t>(r * 8 + c), 0);
      }
    }
  }
  // Black pieces on ranks 5..7 (dark squares)
  for (int r = 5; r < 8; ++r) {
    for (int c = 0; c < 8; ++c) {
      if ((r + c) % 2 == 1) {
        spawnToken(state, static_cast<uint8_t>(r * 8 + c), 1);
      }
    }
  }
}

void toggleSpawnColor(SandboxState& state) {
  state.spawnColor = (state.spawnColor == 0) ? 1 : 0;
}

}  // namespace sandbox
