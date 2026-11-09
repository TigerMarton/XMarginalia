#include "QuoridorCore.h"

namespace quoridor {

void initGame(State& state) {
  state.pawn[0] = 76;  // White: (4, 8)
  state.pawn[1] = 4;   // Black: (4, 0)
  state.wallsLeft[0] = 10;
  state.wallsLeft[1] = 10;
  state.hWalls = 0;
  state.vWalls = 0;
  state.turn = Player::White;
  state.winner = Player::None;
  state.mode = Mode::Solo;
  state.flip = false;
  state.selectedGx = -1;
  state.selectedGy = -1;
  state.curOrient = WallOrientation::Horizontal;
  state.wallPlacementMode = false;
}

bool isBlockedByWall(uint64_t hWalls, uint64_t vWalls, int x1, int y1, int x2, int y2) {
  int dx = x2 - x1;
  int dy = y2 - y1;

  if (dx == 0 && dy == -1) {
    // Moving North
    return hasHWall(hWalls, x1 - 1, y1 - 1) || hasHWall(hWalls, x1, y1 - 1);
  }
  if (dx == 0 && dy == 1) {
    // Moving South
    return hasHWall(hWalls, x1 - 1, y1) || hasHWall(hWalls, x1, y1);
  }
  if (dx == -1 && dy == 0) {
    // Moving West
    return hasVWall(vWalls, x1 - 1, y1 - 1) || hasVWall(vWalls, x1 - 1, y1);
  }
  if (dx == 1 && dy == 0) {
    // Moving East
    return hasVWall(vWalls, x1, y1 - 1) || hasVWall(vWalls, x1, y1);
  }
  return true;
}

int getLegalPawnMoves(const State& state, int8_t* outMoves, int maxMoves) {
  int count = 0;
  int pIdx = static_cast<int>(state.turn);
  int oIdx = static_cast<int>(opponent(state.turn));

  int px = cellX(state.pawn[pIdx]);
  int py = cellY(state.pawn[pIdx]);
  int ox = cellX(state.pawn[oIdx]);
  int oy = cellY(state.pawn[oIdx]);

  constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

  for (const auto& d : dirs) {
    int dx = d[0];
    int dy = d[1];
    int nx = px + dx;
    int ny = py + dy;

    if (nx < 0 || nx >= kCols || ny < 0 || ny >= kRows) continue;
    if (isBlockedByWall(state.hWalls, state.vWalls, px, py, nx, ny)) continue;

    if (nx != ox || ny != oy) {
      if (count < maxMoves) {
        outMoves[count++] = static_cast<int8_t>(cellIdx(nx, ny));
      }
    } else {
      // Opponent pawn is in this adjacent square!
      int jx = nx + dx;
      int jy = ny + dy;
      bool jumped = false;

      if (jx >= 0 && jx < kCols && jy >= 0 && jy < kRows) {
        if (!isBlockedByWall(state.hWalls, state.vWalls, nx, ny, jx, jy)) {
          if (count < maxMoves) {
            outMoves[count++] = static_cast<int8_t>(cellIdx(jx, jy));
          }
          jumped = true;
        }
      }

      if (!jumped) {
        // Diagonal jump fallback
        if (dx == 0) {
          // Moving vertically, sides are x - 1 and x + 1
          int sides[2] = {nx - 1, nx + 1};
          for (int sx : sides) {
            if (sx >= 0 && sx < kCols) {
              if (!isBlockedByWall(state.hWalls, state.vWalls, nx, ny, sx, ny)) {
                if (count < maxMoves) {
                  outMoves[count++] = static_cast<int8_t>(cellIdx(sx, ny));
                }
              }
            }
          }
        } else {
          // Moving horizontally, sides are y - 1 and y + 1
          int sides[2] = {ny - 1, ny + 1};
          for (int sy : sides) {
            if (sy >= 0 && sy < kRows) {
              if (!isBlockedByWall(state.hWalls, state.vWalls, nx, ny, nx, sy)) {
                if (count < maxMoves) {
                  outMoves[count++] = static_cast<int8_t>(cellIdx(nx, sy));
                }
              }
            }
          }
        }
      }
    }
  }

  return count;
}

bool isLegalPawnMove(const State& state, int targetCell) {
  int8_t moves[8];
  int count = getLegalPawnMoves(state, moves, 8);
  for (int i = 0; i < count; ++i) {
    if (moves[i] == targetCell) return true;
  }
  return false;
}

bool hasPathToGoal(const State& state, Player p) {
  int targetRow = (p == Player::White) ? 0 : (kRows - 1);
  int startCell = state.pawn[static_cast<int>(p)];

  int8_t queue[kCells];
  bool visited[kCells] = {false};
  int head = 0;
  int tail = 0;

  queue[tail++] = static_cast<int8_t>(startCell);
  visited[startCell] = true;

  constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

  while (head < tail) {
    int curr = queue[head++];
    int cx = cellX(curr);
    int cy = cellY(curr);

    if (cy == targetRow) return true;

    for (const auto& d : dirs) {
      int nx = cx + d[0];
      int ny = cy + d[1];
      if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
        int nextCell = cellIdx(nx, ny);
        if (!visited[nextCell] && !isBlockedByWall(state.hWalls, state.vWalls, cx, cy, nx, ny)) {
          visited[nextCell] = true;
          queue[tail++] = static_cast<int8_t>(nextCell);
        }
      }
    }
  }

  return false;
}

int getShortestPathLength(const State& state, Player p) {
  int targetRow = (p == Player::White) ? 0 : (kRows - 1);
  int startCell = state.pawn[static_cast<int>(p)];

  int8_t queue[kCells];
  int8_t dist[kCells];
  for (int i = 0; i < kCells; ++i) dist[i] = -1;

  int head = 0;
  int tail = 0;

  queue[tail++] = static_cast<int8_t>(startCell);
  dist[startCell] = 0;

  constexpr int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

  while (head < tail) {
    int curr = queue[head++];
    int cx = cellX(curr);
    int cy = cellY(curr);

    if (cy == targetRow) return dist[curr];

    for (const auto& d : dirs) {
      int nx = cx + d[0];
      int ny = cy + d[1];
      if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
        int nextCell = cellIdx(nx, ny);
        if (dist[nextCell] == -1 && !isBlockedByWall(state.hWalls, state.vWalls, cx, cy, nx, ny)) {
          dist[nextCell] = static_cast<int8_t>(dist[curr] + 1);
          queue[tail++] = static_cast<int8_t>(nextCell);
        }
      }
    }
  }

  return -1;
}

bool canPlaceWall(const State& state, int gx, int gy, WallOrientation orient) {
  if (state.winner != Player::None) return false;
  int pIdx = static_cast<int>(state.turn);
  if (state.wallsLeft[pIdx] == 0) return false;

  if (gx < 0 || gx >= kInterCols || gy < 0 || gy >= kInterRows) return false;

  if (orient == WallOrientation::Horizontal) {
    if (hasHWall(state.hWalls, gx, gy)) return false;
    if (gx > 0 && hasHWall(state.hWalls, gx - 1, gy)) return false;
    if (gx < kInterCols - 1 && hasHWall(state.hWalls, gx + 1, gy)) return false;
    if (hasVWall(state.vWalls, gx, gy)) return false;  // crossing
  } else {
    if (hasVWall(state.vWalls, gx, gy)) return false;
    if (gy > 0 && hasVWall(state.vWalls, gx, gy - 1)) return false;
    if (gy < kInterRows - 1 && hasVWall(state.vWalls, gx, gy + 1)) return false;
    if (hasHWall(state.hWalls, gx, gy)) return false;  // crossing
  }

  // Fair-play validation: check paths
  State copy = state;
  if (orient == WallOrientation::Horizontal) {
    copy.hWalls |= (1ULL << interIdx(gx, gy));
  } else {
    copy.vWalls |= (1ULL << interIdx(gx, gy));
  }

  return hasPathToGoal(copy, Player::White) && hasPathToGoal(copy, Player::Black);
}

bool executePawnMove(State& state, int targetCell) {
  if (state.winner != Player::None) return false;
  if (!isLegalPawnMove(state, targetCell)) return false;

  int pIdx = static_cast<int>(state.turn);
  state.pawn[pIdx] = static_cast<int8_t>(targetCell);

  int targetRow = (state.turn == Player::White) ? 0 : (kRows - 1);
  if (cellY(targetCell) == targetRow) {
    state.winner = state.turn;
    return true;
  }

  state.turn = opponent(state.turn);
  state.selectedGx = -1;
  state.selectedGy = -1;
  state.wallPlacementMode = false;
  return true;
}

bool executeWallPlacement(State& state, int gx, int gy, WallOrientation orient) {
  if (!canPlaceWall(state, gx, gy, orient)) return false;

  int pIdx = static_cast<int>(state.turn);
  state.wallsLeft[pIdx]--;

  if (orient == WallOrientation::Horizontal) {
    state.hWalls |= (1ULL << interIdx(gx, gy));
  } else {
    state.vWalls |= (1ULL << interIdx(gx, gy));
  }

  state.turn = opponent(state.turn);
  state.selectedGx = -1;
  state.selectedGy = -1;
  state.wallPlacementMode = false;
  return true;
}

bool executeMove(State& state, const Move& move) {
  if (move.isWall) {
    return executeWallPlacement(state, move.wallGx, move.wallGy, move.wallOrient);
  } else {
    return executePawnMove(state, move.targetCell);
  }
}

}  // namespace quoridor
