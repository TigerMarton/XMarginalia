#pragma once

#include <cstddef>
#include <cstdint>

namespace santorini {

constexpr int kCols = 5;
constexpr int kRows = 5;
constexpr int kCells = kCols * kRows;  // 25

enum class Player : uint8_t {
  White = 0,
  Black = 1,
  None = 2,
};

inline Player opponent(Player p) {
  return (p == Player::White) ? Player::Black : Player::White;
}

enum class Phase : uint8_t {
  SelectWorker = 0,
  SelectMove = 1,
  SelectBuild = 2,
  GameOver = 3,
};

enum class Mode : uint8_t {
  Solo = 0,
  TwoPlayer = 1,
  Nearby = 2,
};

struct Move {
  int8_t worker;     // 0 or 1
  int8_t moveCell;   // destination cell 0..24
  int8_t buildCell;  // build cell 0..24
};

struct State {
  uint8_t board[kCells];  // 0 = Ground, 1..3 = Levels 1..3, 4 = Dome
  int8_t workers[2][2];   // [Player 0..1][Worker 0..1] = cell 0..24
  Player turn;
  Phase phase;
  int8_t selectedWorker;  // 0 or 1, or -1
  Player winner;
  Mode mode;
  bool flip;
};

inline int cellX(int cell) { return cell % kCols; }
inline int cellY(int cell) { return cell / kCols; }
inline int cellIdx(int x, int y) { return y * kCols + x; }

inline bool isAdjacent(int c1, int c2) {
  if (c1 < 0 || c1 >= kCells || c2 < 0 || c2 >= kCells || c1 == c2) return false;
  int dx = cellX(c1) - cellX(c2);
  if (dx < 0) dx = -dx;
  int dy = cellY(c1) - cellY(c2);
  if (dy < 0) dy = -dy;
  return dx <= 1 && dy <= 1;
}

void initGame(State& state);
int workerAt(const State& state, int cell, Player& outOwner);
bool canWorkerMove(const State& state, Player p, int workerIdx);
bool hasAnyLegalMoves(const State& state, Player p);
bool isLegalMove(const State& state, int workerIdx, int targetCell);
bool isLegalBuild(const State& state, int buildCell);

bool selectWorker(State& state, int workerIdx);
bool executeMove(State& state, int targetCell);
bool executeBuild(State& state, int buildCell);
bool executeFullMove(State& state, const Move& move);

int getLegalMovesForWorker(const State& state, int workerIdx, int8_t* outMoves, int maxMoves);
int getLegalBuilds(const State& state, int8_t* outBuilds, int maxBuilds);

}  // namespace santorini
