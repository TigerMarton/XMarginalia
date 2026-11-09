#pragma once

#include <cstddef>
#include <cstdint>

namespace quoridor {

constexpr int kCols = 9;
constexpr int kRows = 9;
constexpr int kCells = kCols * kRows;  // 81

constexpr int kInterCols = 8;
constexpr int kInterRows = 8;
constexpr int kIntersections = kInterCols * kInterRows;  // 64

enum class Player : uint8_t {
  White = 0,
  Black = 1,
  None = 2,
};

inline Player opponent(Player p) {
  return (p == Player::White) ? Player::Black : Player::White;
}

enum class WallOrientation : uint8_t {
  Horizontal = 0,
  Vertical = 1,
};

enum class Mode : uint8_t {
  Solo = 0,
  TwoPlayer = 1,
  Nearby = 2,
};

struct Move {
  bool isWall;                // false = pawn move, true = wall placement
  int8_t targetCell;          // 0..80 for pawn move
  int8_t wallGx;              // 0..7
  int8_t wallGy;              // 0..7
  WallOrientation wallOrient; // Horizontal or Vertical
};

struct State {
  int8_t pawn[2];             // pawn[0] = White (0..80), pawn[1] = Black (0..80)
  uint8_t wallsLeft[2];       // remaining walls (starts at 10)
  uint64_t hWalls;            // bitmask of 8x8 horizontal walls
  uint64_t vWalls;            // bitmask of 8x8 vertical walls
  Player turn;
  Player winner;
  Mode mode;
  bool flip;
  // UI selection state
  int8_t selectedGx;          // 0..7, or -1
  int8_t selectedGy;          // 0..7, or -1
  WallOrientation curOrient;
  bool wallPlacementMode;     // true if placing a wall, false if moving pawn
};

inline int cellX(int cell) { return cell % kCols; }
inline int cellY(int cell) { return cell / kCols; }
inline int cellIdx(int x, int y) { return y * kCols + x; }

inline int interIdx(int gx, int gy) { return gy * kInterCols + gx; }

inline bool hasHWall(uint64_t hWalls, int gx, int gy) {
  if (gx < 0 || gx >= kInterCols || gy < 0 || gy >= kInterRows) return false;
  return (hWalls & (1ULL << interIdx(gx, gy))) != 0;
}

inline bool hasVWall(uint64_t vWalls, int gx, int gy) {
  if (gx < 0 || gx >= kInterCols || gy < 0 || gy >= kInterRows) return false;
  return (vWalls & (1ULL << interIdx(gx, gy))) != 0;
}

void initGame(State& state);

bool isBlockedByWall(uint64_t hWalls, uint64_t vWalls, int x1, int y1, int x2, int y2);
int getLegalPawnMoves(const State& state, int8_t* outMoves, int maxMoves);
bool isLegalPawnMove(const State& state, int targetCell);

bool canPlaceWall(const State& state, int gx, int gy, WallOrientation orient);
bool hasPathToGoal(const State& state, Player p);
int getShortestPathLength(const State& state, Player p);

bool executePawnMove(State& state, int targetCell);
bool executeWallPlacement(State& state, int gx, int gy, WallOrientation orient);
bool executeMove(State& state, const Move& move);

}  // namespace quoridor
