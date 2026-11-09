#pragma once

#include <cstdint>
#include <cstddef>

namespace dobutsu {

constexpr int kCols = 3;
constexpr int kRows = 4;
constexpr int kCells = kCols * kRows;  // 12 cells

enum class PieceType : uint8_t {
  None = 0,
  Lion = 1,      // Moves 1 step in all 8 directions
  Giraffe = 2,   // Moves 1 step orthogonally (4 directions)
  Elephant = 3,  // Moves 1 step diagonally (4 directions)
  Chick = 4,     // Moves 1 step forward
  Hen = 5,       // Promoted chick: 4 orthogonal + 2 forward diagonals
};

enum class Player : uint8_t {
  None = 0,
  P1 = 1,  // South (moves UP towards row 0)
  P2 = 2,  // North (moves DOWN towards row 3)
};

// Packed cell: high nibble = Player (1 or 2), low nibble = PieceType (1..5)
// 0 = empty cell
using Cell = uint8_t;

inline Cell makePiece(Player p, PieceType t) {
  return static_cast<Cell>((static_cast<uint8_t>(p) << 4) | (static_cast<uint8_t>(t) & 0x0F));
}

inline Player pieceOwner(Cell c) {
  return static_cast<Player>(c >> 4);
}

inline PieceType pieceType(Cell c) {
  return static_cast<PieceType>(c & 0x0F);
}

inline bool isP1(Cell c) {
  return pieceOwner(c) == Player::P1;
}

inline bool isP2(Cell c) {
  return pieceOwner(c) == Player::P2;
}

// Special drop constants for Move.from
constexpr int8_t kDropGiraffe = -1;
constexpr int8_t kDropElephant = -2;
constexpr int8_t kDropChick = -3;

inline PieceType dropTypeFromIndex(int8_t dropIdx) {
  if (dropIdx == kDropGiraffe) return PieceType::Giraffe;
  if (dropIdx == kDropElephant) return PieceType::Elephant;
  if (dropIdx == kDropChick) return PieceType::Chick;
  return PieceType::None;
}

inline int8_t dropIndexFromType(PieceType type) {
  if (type == PieceType::Giraffe) return kDropGiraffe;
  if (type == PieceType::Elephant) return kDropElephant;
  if (type == PieceType::Chick) return kDropChick;
  return 0;
}

inline size_t reserveIndex(PieceType type) {
  if (type == PieceType::Giraffe) return 0;
  if (type == PieceType::Elephant) return 1;
  return 2;  // Chick
}

struct Move {
  int8_t from = 0;  // 0..11 for board piece, or kDropGiraffe / kDropElephant / kDropChick
  int8_t to = 0;    // 0..11
  bool promotes = false;

  bool isDrop() const { return from < 0; }
  bool operator==(const Move& o) const {
    return from == o.from && to == o.to && promotes == o.promotes;
  }
};

struct Game {
  Cell board[kCells] = {};
  // Reserve counts: [player 0=P1, 1=P2][0=Giraffe, 1=Elephant, 2=Chick]
  uint8_t reserves[2][3] = {};
  Player turn = Player::P1;
  Player winner = Player::None;
  bool isOver = false;
  int8_t lionTryPlayer = 0;  // 1 if P1 reached row 0 and awaits P2 reply, 2 if P2 reached row 3
};

void initGame(Game& game);
bool isLegalMove(const Game& game, const Move& move);
bool applyMove(Game& game, const Move& move);
size_t generateLegalMoves(const Game& game, Move* outMoves, size_t maxMoves);
bool canAttackSquare(const Game& game, Player attacker, int8_t targetSq);

inline Player opponent(Player p) {
  if (p == Player::P1) return Player::P2;
  if (p == Player::P2) return Player::P1;
  return Player::None;
}

}  // namespace dobutsu
