#include "MancalaCore.h"

namespace mancala {

void start(Game& game) {
  for (int i = 0; i < 6; ++i) {
    game.pits[i] = kSeedsPerPit;
  }
  game.pits[kStore1] = 0;

  for (int i = 7; i < 13; ++i) {
    game.pits[i] = kSeedsPerPit;
  }
  game.pits[kStore2] = 0;

  game.turn = kPlayer1;
  game.isOver = false;
  game.extraTurn = false;
  game.lastMove = -1;
}

int playerPitSeeds(const Game& game, const uint8_t player) {
  int total = 0;
  const int start = (player == kPlayer1) ? 0 : 7;
  for (int i = 0; i < 6; ++i) {
    total += game.pits[start + i];
  }
  return total;
}

bool hasLegalMoves(const Game& game, const uint8_t player) {
  if (game.isOver) return false;
  return playerPitSeeds(game, player) > 0;
}

bool isLegalMove(const Game& game, const uint8_t player, const int pit) {
  if (game.isOver) return false;
  if (!isFriendlyPit(player, pit)) return false;
  return game.pits[pit] > 0;
}

bool playMove(Game& game, const int pit) {
  if (!isLegalMove(game, game.turn, pit)) return false;

  int seeds = game.pits[pit];
  game.pits[pit] = 0;
  game.lastMove = pit;
  game.extraTurn = false;

  const int oppStore = opponentStore(game.turn);
  const int ownStore = storeOf(game.turn);
  int current = pit;

  while (seeds > 0) {
    current = (current + 1) % kPitsTotal;
    if (current == oppStore) {
      continue;
    }
    game.pits[current]++;
    seeds--;
  }

  // Check last seed landing
  if (current == ownStore) {
    game.extraTurn = true;
  } else if (isFriendlyPit(game.turn, current) && game.pits[current] == 1) {
    // Landed in an empty friendly pit; capture if opposite has seeds
    const int opp = oppositePit(current);
    if (opp >= 0 && game.pits[opp] > 0) {
      game.pits[ownStore] += (1 + game.pits[opp]);
      game.pits[current] = 0;
      game.pits[opp] = 0;
    }
  }

  // Turn management
  if (!game.extraTurn) {
    game.turn = opponent(game.turn);
  }

  // Check end game: if either player has 0 seeds in pits
  const int p1Seeds = playerPitSeeds(game, kPlayer1);
  const int p2Seeds = playerPitSeeds(game, kPlayer2);
  if (p1Seeds == 0 || p2Seeds == 0) {
    for (int i = 0; i < 6; ++i) {
      game.pits[kStore1] += game.pits[i];
      game.pits[i] = 0;
    }
    for (int i = 7; i < 13; ++i) {
      game.pits[kStore2] += game.pits[i];
      game.pits[i] = 0;
    }
    game.isOver = true;
    game.extraTurn = false;
  }

  return true;
}

Outcome outcome(const Game& game) {
  if (!game.isOver) return Outcome::InProgress;
  const int s1 = game.pits[kStore1];
  const int s2 = game.pits[kStore2];
  if (s1 > s2) return Outcome::Player1Won;
  if (s2 > s1) return Outcome::Player2Won;
  return Outcome::Draw;
}

}  // namespace mancala
