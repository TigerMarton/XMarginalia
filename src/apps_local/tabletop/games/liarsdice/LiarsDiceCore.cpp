#include "LiarsDiceCore.h"

namespace liarsdice {

namespace {

uint32_t xorshift32(uint32_t& state) {
  uint32_t x = state;
  if (x == 0) x = 0x853c49e6;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  state = x;
  return x;
}

uint8_t rollD6(uint32_t& state) {
  return static_cast<uint8_t>((xorshift32(state) % 6) + 1);
}

}  // namespace

void initGame(Game& game, uint8_t numPlayers, uint32_t seed) {
  if (numPlayers < 2) numPlayers = 2;
  if (numPlayers > kMaxPlayers) numPlayers = kMaxPlayers;

  game.playerCount = numPlayers;
  game.currentBid = Bid{0, 0, 0};
  game.activePlayer = 0;
  game.phase = Phase::Bidding;
  game.challenger = 0;
  game.roundLoser = 0;
  game.totalMatchingDice = 0;
  game.challengeSuccessful = false;
  game.winner = 0;

  uint32_t rng = seed ? seed : 0x12345678;

  for (uint8_t p = 0; p < numPlayers; ++p) {
    game.players[p].diceCount = kStartingDice;
    game.players[p].isAi = (p > 0);
    for (uint8_t d = 0; d < kStartingDice; ++d) {
      game.players[p].dice[d] = rollD6(rng);
    }
  }
}

void rollPlayerDice(Game& game, uint8_t playerIdx, uint32_t seed) {
  if (playerIdx >= game.playerCount) return;
  uint32_t rng = seed ? seed : (0x9e3779b9 + playerIdx * 101);
  for (uint8_t d = 0; d < game.players[playerIdx].diceCount; ++d) {
    game.players[playerIdx].dice[d] = rollD6(rng);
  }
}

void startNewRound(Game& game, uint32_t seed) {
  if (game.phase == Phase::GameOver) return;

  game.currentBid = Bid{0, 0, 0};
  game.challenger = 0;
  game.totalMatchingDice = 0;
  game.challengeSuccessful = false;
  game.phase = Phase::Bidding;

  // The round loser starts, unless eliminated
  if (game.players[game.roundLoser].diceCount == 0) {
    game.activePlayer = getNextActivePlayer(game, game.roundLoser);
  } else {
    game.activePlayer = game.roundLoser;
  }

  uint32_t rng = seed ? seed : (0xa5a5a5a5 ^ (game.activePlayer * 31));
  for (uint8_t p = 0; p < game.playerCount; ++p) {
    if (game.players[p].diceCount > 0) {
      for (uint8_t d = 0; d < game.players[p].diceCount; ++d) {
        game.players[p].dice[d] = rollD6(rng);
      }
    }
  }
}

bool isValidBid(const Game& game, const Bid& nextBid) {
  if (game.phase != Phase::Bidding) return false;
  if (nextBid.face < 1 || nextBid.face > 6) return false;
  if (nextBid.count < 1) return false;

  const uint8_t totalDice = countTotalActiveDice(game);
  if (nextBid.count > totalDice) return false;

  // First bid of round
  if (game.currentBid.count == 0) {
    return true;
  }

  const Bid& cur = game.currentBid;

  if (cur.face != 1) {
    if (nextBid.face != 1) {
      if (nextBid.count > cur.count) return true;
      if (nextBid.count == cur.count && nextBid.face > cur.face) return true;
      return false;
    } else {
      // Shifting to 1s (Aces)
      const uint8_t minAces = static_cast<uint8_t>((cur.count + 1) / 2);
      return nextBid.count >= minAces;
    }
  } else {
    // Current bid is on 1s (Aces)
    if (nextBid.face == 1) {
      return nextBid.count > cur.count;
    } else {
      // Shifting from 1s to numbers
      const uint8_t minNumbers = static_cast<uint8_t>(cur.count * 2 + 1);
      return nextBid.count >= minNumbers;
    }
  }
}

bool makeBid(Game& game, const Bid& nextBid) {
  if (!isValidBid(game, nextBid)) return false;

  game.currentBid = nextBid;
  game.currentBid.bidder = game.activePlayer;
  game.activePlayer = getNextActivePlayer(game, game.activePlayer);
  return true;
}

uint8_t countTotalMatching(const Game& game, uint8_t face) {
  uint8_t total = 0;
  for (uint8_t p = 0; p < game.playerCount; ++p) {
    if (game.players[p].diceCount == 0) continue;
    for (uint8_t d = 0; d < game.players[p].diceCount; ++d) {
      const uint8_t val = game.players[p].dice[d];
      if (face == 1) {
        if (val == 1) total++;
      } else {
        if (val == face || val == 1) total++;
      }
    }
  }
  return total;
}

uint8_t countTotalActiveDice(const Game& game) {
  uint8_t total = 0;
  for (uint8_t p = 0; p < game.playerCount; ++p) {
    total = static_cast<uint8_t>(total + game.players[p].diceCount);
  }
  return total;
}

uint8_t getNextActivePlayer(const Game& game, uint8_t current) {
  for (uint8_t i = 1; i <= game.playerCount; ++i) {
    const uint8_t next = static_cast<uint8_t>((current + i) % game.playerCount);
    if (game.players[next].diceCount > 0) {
      return next;
    }
  }
  return current;
}

bool challenge(Game& game) {
  if (game.phase != Phase::Bidding) return false;
  if (game.currentBid.count == 0) return false;

  game.challenger = game.activePlayer;
  game.totalMatchingDice = countTotalMatching(game, game.currentBid.face);

  if (game.totalMatchingDice < game.currentBid.count) {
    // Bid was a lie / bluff
    game.challengeSuccessful = true;
    game.roundLoser = game.currentBid.bidder;
  } else {
    // Bid was true / safe
    game.challengeSuccessful = false;
    game.roundLoser = game.challenger;
  }

  // Deduct 1 die from loser
  if (game.players[game.roundLoser].diceCount > 0) {
    game.players[game.roundLoser].diceCount--;
  }

  // Check how many players remain with dice
  uint8_t activePlayers = 0;
  uint8_t lastStanding = 0;
  for (uint8_t p = 0; p < game.playerCount; ++p) {
    if (game.players[p].diceCount > 0) {
      activePlayers++;
      lastStanding = p;
    }
  }

  if (activePlayers <= 1) {
    game.phase = Phase::GameOver;
    game.winner = lastStanding;
  } else {
    game.phase = Phase::Showdown;
  }

  return true;
}

}  // namespace liarsdice
