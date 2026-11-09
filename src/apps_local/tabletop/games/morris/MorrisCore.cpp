#include "MorrisCore.h"

namespace morris {

const uint32_t kAdjacentMask[24] = {
    // Outer (0..7)
    (1U << 1) | (1U << 7),                              // 0
    (1U << 0) | (1U << 2) | (1U << 9),                  // 1
    (1U << 1) | (1U << 3),                              // 2
    (1U << 2) | (1U << 4) | (1U << 11),                 // 3
    (1U << 3) | (1U << 5),                              // 4
    (1U << 4) | (1U << 6) | (1U << 13),                 // 5
    (1U << 5) | (1U << 7),                              // 6
    (1U << 6) | (1U << 0) | (1U << 15),                 // 7

    // Middle (8..15)
    (1U << 9) | (1U << 15),                             // 8
    (1U << 8) | (1U << 10) | (1U << 1) | (1U << 17),    // 9
    (1U << 9) | (1U << 11),                             // 10
    (1U << 10) | (1U << 12) | (1U << 3) | (1U << 19),   // 11
    (1U << 11) | (1U << 13),                            // 12
    (1U << 12) | (1U << 14) | (1U << 5) | (1U << 21),   // 13
    (1U << 13) | (1U << 15),                            // 14
    (1U << 14) | (1U << 8) | (1U << 7) | (1U << 23),    // 15

    // Inner (16..23)
    (1U << 17) | (1U << 23),                            // 16
    (1U << 16) | (1U << 18) | (1U << 9),                // 17
    (1U << 17) | (1U << 19),                            // 18
    (1U << 18) | (1U << 20) | (1U << 11),               // 19
    (1U << 19) | (1U << 21),                            // 20
    (1U << 20) | (1U << 22) | (1U << 13),               // 21
    (1U << 21) | (1U << 23),                            // 22
    (1U << 22) | (1U << 16) | (1U << 15),               // 23
};

const uint32_t kMills[16] = {
    // Outer square edges
    (1U << 0) | (1U << 1) | (1U << 2),     // 0: top
    (1U << 2) | (1U << 3) | (1U << 4),     // 1: right
    (1U << 4) | (1U << 5) | (1U << 6),     // 2: bottom
    (1U << 6) | (1U << 7) | (1U << 0),     // 3: left

    // Middle square edges
    (1U << 8) | (1U << 9) | (1U << 10),    // 4: top
    (1U << 10) | (1U << 11) | (1U << 12),  // 5: right
    (1U << 12) | (1U << 13) | (1U << 14),  // 6: bottom
    (1U << 14) | (1U << 15) | (1U << 8),   // 7: left

    // Inner square edges
    (1U << 16) | (1U << 17) | (1U << 18),  // 8: top
    (1U << 18) | (1U << 19) | (1U << 20),  // 9: right
    (1U << 20) | (1U << 21) | (1U << 22),  // 10: bottom
    (1U << 22) | (1U << 23) | (1U << 16),  // 11: left

    // Radial cross lines
    (1U << 1) | (1U << 9) | (1U << 17),    // 12: top radial
    (1U << 3) | (1U << 11) | (1U << 19),   // 13: right radial
    (1U << 5) | (1U << 13) | (1U << 21),   // 14: bottom radial
    (1U << 7) | (1U << 15) | (1U << 23),   // 15: left radial
};

const uint8_t kVertexMills[24][2] = {
    // Outer
    {0, 3}, {0, 12}, {0, 1}, {1, 13}, {1, 2}, {2, 14}, {2, 3}, {3, 15},
    // Middle
    {4, 7}, {4, 12}, {4, 5}, {5, 13}, {5, 6}, {6, 14}, {6, 7}, {7, 15},
    // Inner
    {8, 11}, {8, 12}, {8, 9}, {9, 13}, {9, 10}, {10, 14}, {10, 11}, {11, 15},
};

void start(Game& game) {
  game.pieces[kWhite] = 0;
  game.pieces[kBlack] = 0;
  game.placed[kWhite] = 0;
  game.placed[kBlack] = 0;
  game.turn = kWhite;
  game.isOver = false;
  game.pendingRemoval = false;
  game.selected = -1;
}

Phase playerPhase(const Game& game, const uint8_t player) {
  if (game.placed[player] < kMenPerPlayer) {
    return Phase::Placing;
  }
  if (pieceCount(game.pieces[player]) <= 3) {
    return Phase::Flying;
  }
  return Phase::Moving;
}

bool isPartOfMill(const uint32_t playerPieces, const int vertex) {
  if (vertex < 0 || vertex >= kVertices) return false;
  if (!(playerPieces & (1U << vertex))) return false;
  const uint32_t m0 = kMills[kVertexMills[vertex][0]];
  if ((playerPieces & m0) == m0) return true;
  const uint32_t m1 = kMills[kVertexMills[vertex][1]];
  if ((playerPieces & m1) == m1) return true;
  return false;
}

bool canRemovePiece(const Game& game, const uint8_t enemy, const int vertex) {
  if (vertex < 0 || vertex >= kVertices) return false;
  const uint32_t enemyPieces = game.pieces[enemy];
  if (!(enemyPieces & (1U << vertex))) return false;

  if (!isPartOfMill(enemyPieces, vertex)) {
    return true;
  }
  // Check if ALL enemy pieces on board are currently in mills
  for (int v = 0; v < kVertices; ++v) {
    if ((enemyPieces & (1U << v)) && !isPartOfMill(enemyPieces, v)) {
      return false;
    }
  }
  return true;
}

bool hasLegalMoves(const Game& game, const uint8_t player) {
  if (game.isOver) return false;
  const Phase phase = playerPhase(game, player);
  const uint32_t occupied = game.pieces[kWhite] | game.pieces[kBlack];
  if (phase == Phase::Placing || phase == Phase::Flying) {
    return occupied != 0x00FFFFFF;
  }

  // Moving phase
  const uint32_t myPieces = game.pieces[player];
  for (int v = 0; v < kVertices; ++v) {
    if (myPieces & (1U << v)) {
      if ((kAdjacentMask[v] & ~occupied) != 0) {
        return true;
      }
    }
  }
  return false;
}

uint32_t legalPlacements(const Game& game) {
  if (game.isOver || game.pendingRemoval) return 0;
  if (playerPhase(game, game.turn) != Phase::Placing) return 0;
  const uint32_t occupied = game.pieces[kWhite] | game.pieces[kBlack];
  return 0x00FFFFFF & ~occupied;
}

uint32_t legalDestinations(const Game& game, const int fromVertex) {
  if (game.isOver || game.pendingRemoval) return 0;
  if (fromVertex < 0 || fromVertex >= kVertices) return 0;
  if (!(game.pieces[game.turn] & (1U << fromVertex))) return 0;

  const uint32_t occupied = game.pieces[kWhite] | game.pieces[kBlack];
  const Phase phase = playerPhase(game, game.turn);
  if (phase == Phase::Flying) {
    return 0x00FFFFFF & ~occupied;
  }
  if (phase == Phase::Moving) {
    return kAdjacentMask[fromVertex] & ~occupied;
  }
  return 0;
}

uint32_t removablePieces(const Game& game, const uint8_t enemy) {
  if (!game.pendingRemoval || game.isOver) return 0;
  uint32_t mask = 0;
  for (int v = 0; v < kVertices; ++v) {
    if (canRemovePiece(game, enemy, v)) {
      mask |= (1U << v);
    }
  }
  return mask;
}

namespace {

void checkGameOver(Game& game) {
  const uint8_t next = game.turn;
  const int count = pieceCount(game.pieces[next]);
  if (game.placed[next] == kMenPerPlayer && count < 3) {
    game.isOver = true;
    return;
  }
  if (!hasLegalMoves(game, next)) {
    game.isOver = true;
    return;
  }
}

}  // namespace

bool placePiece(Game& game, const int vertex) {
  if (game.isOver || game.pendingRemoval) return false;
  if (playerPhase(game, game.turn) != Phase::Placing) return false;
  if (vertex < 0 || vertex >= kVertices) return false;

  const uint32_t occupied = game.pieces[kWhite] | game.pieces[kBlack];
  if (occupied & (1U << vertex)) return false;

  game.pieces[game.turn] |= (1U << vertex);
  game.placed[game.turn]++;

  if (isPartOfMill(game.pieces[game.turn], vertex)) {
    game.pendingRemoval = true;
  } else {
    game.turn = opponent(game.turn);
    checkGameOver(game);
  }

  return true;
}

bool movePiece(Game& game, const int from, const int to) {
  if (game.isOver || game.pendingRemoval) return false;
  if (from < 0 || from >= kVertices || to < 0 || to >= kVertices) return false;
  if (!(game.pieces[game.turn] & (1U << from))) return false;

  const uint32_t dests = legalDestinations(game, from);
  if (!(dests & (1U << to))) return false;

  game.pieces[game.turn] &= ~(1U << from);
  game.pieces[game.turn] |= (1U << to);
  game.selected = -1;

  if (isPartOfMill(game.pieces[game.turn], to)) {
    game.pendingRemoval = true;
  } else {
    game.turn = opponent(game.turn);
    checkGameOver(game);
  }

  return true;
}

bool removePiece(Game& game, const int vertex) {
  if (game.isOver || !game.pendingRemoval) return false;
  const uint8_t enemy = opponent(game.turn);
  if (!canRemovePiece(game, enemy, vertex)) return false;

  game.pieces[enemy] &= ~(1U << vertex);
  game.pendingRemoval = false;

  game.turn = enemy;
  checkGameOver(game);
  return true;
}

Outcome outcome(const Game& game) {
  if (!game.isOver) return Outcome::InProgress;
  // If game is over, the player whose turn it is lost!
  return (game.turn == kWhite) ? Outcome::BlackWon : Outcome::WhiteWon;
}

}  // namespace morris
