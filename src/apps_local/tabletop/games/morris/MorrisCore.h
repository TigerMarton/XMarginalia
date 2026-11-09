#pragma once

#include <cstdint>

namespace morris {

constexpr int kVertices = 24;
constexpr int kMenPerPlayer = 9;

constexpr uint8_t kNone = 0;
constexpr uint8_t kWhite = 1;  // Player 1 (Human)
constexpr uint8_t kBlack = 2;  // Player 2 (AI)

inline uint8_t opponent(const uint8_t player) {
  return player == kWhite ? kBlack : kWhite;
}

enum class Phase : uint8_t {
  Placing,  // Placing men on empty vertices (up to 9 each)
  Moving,   // Sliding men to adjacent vacant vertices
  Flying,   // Reduced to 3 men: can jump to any vacant vertex
};

enum class Outcome : uint8_t {
  InProgress,
  WhiteWon,
  BlackWon,
  Draw,
};

struct Game {
  uint32_t pieces[3] = {};   // 1: White, 2: Black
  uint8_t placed[3] = {};    // Count of placed men (0..9)
  uint8_t turn = kWhite;
  bool isOver = false;
  bool pendingRemoval = false;  // Mill formed: current player must remove enemy piece
  int8_t selected = -1;         // Selected piece for move/fly (-1 if none)
};

extern const uint32_t kAdjacentMask[24];
extern const uint32_t kMills[16];
extern const uint8_t kVertexMills[24][2];

inline int pieceCount(const uint32_t bitmask) {
  return __builtin_popcount(bitmask);
}

inline uint8_t pieceAt(const Game& game, const int vertex) {
  if (vertex < 0 || vertex >= kVertices) return kNone;
  const uint32_t mask = 1U << vertex;
  if (game.pieces[kWhite] & mask) return kWhite;
  if (game.pieces[kBlack] & mask) return kBlack;
  return kNone;
}

Phase playerPhase(const Game& game, uint8_t player);
bool isPartOfMill(uint32_t playerPieces, int vertex);
bool canRemovePiece(const Game& game, uint8_t enemy, int vertex);
bool hasLegalMoves(const Game& game, uint8_t player);

uint32_t legalPlacements(const Game& game);
uint32_t legalDestinations(const Game& game, int fromVertex);
uint32_t removablePieces(const Game& game, uint8_t enemy);

void start(Game& game);
bool placePiece(Game& game, int vertex);
bool movePiece(Game& game, int from, int to);
bool removePiece(Game& game, int vertex);

Outcome outcome(const Game& game);

}  // namespace morris
