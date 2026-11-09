#include "SantoriniCore.h"

namespace santorini {

void initGame(State& state) {
  for (int i = 0; i < kCells; ++i) {
    state.board[i] = 0;
  }
  // P1 (White) standard starting squares
  state.workers[0][0] = 6;   // (1, 1)
  state.workers[0][1] = 18;  // (3, 3)

  // P2 (Black) standard starting squares
  state.workers[1][0] = 8;   // (3, 1)
  state.workers[1][1] = 16;  // (1, 3)

  state.turn = Player::White;
  state.phase = Phase::SelectWorker;
  state.selectedWorker = -1;
  state.winner = Player::None;
  state.mode = Mode::Solo;
  state.flip = false;
}

int workerAt(const State& state, int cell, Player& outOwner) {
  if (cell < 0 || cell >= kCells) return -1;
  for (int p = 0; p < 2; ++p) {
    for (int w = 0; w < 2; ++w) {
      if (state.workers[p][w] == cell) {
        outOwner = static_cast<Player>(p);
        return w;
      }
    }
  }
  return -1;
}

bool isLegalMove(const State& state, int workerIdx, int targetCell) {
  if (workerIdx < 0 || workerIdx > 1) return false;
  if (targetCell < 0 || targetCell >= kCells) return false;

  int from = state.workers[static_cast<int>(state.turn)][workerIdx];
  if (!isAdjacent(from, targetCell)) return false;

  Player owner = Player::None;
  if (workerAt(state, targetCell, owner) != -1) return false;  // occupied

  if (state.board[targetCell] >= 4) return false;  // dome

  if (state.board[targetCell] > state.board[from] + 1) return false;  // climb max +1

  return true;
}

bool canWorkerMove(const State& state, Player p, int workerIdx) {
  if (workerIdx < 0 || workerIdx > 1) return false;
  int from = state.workers[static_cast<int>(p)][workerIdx];
  int fx = cellX(from);
  int fy = cellY(from);

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      int nx = fx + dx;
      int ny = fy + dy;
      if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
        int target = cellIdx(nx, ny);
        Player owner = Player::None;
        if (workerAt(state, target, owner) != -1) continue;
        if (state.board[target] >= 4) continue;
        if (state.board[target] <= state.board[from] + 1) {
          return true;
        }
      }
    }
  }
  return false;
}

bool hasAnyLegalMoves(const State& state, Player p) {
  return canWorkerMove(state, p, 0) || canWorkerMove(state, p, 1);
}

bool isLegalBuild(const State& state, int buildCell) {
  if (state.phase != Phase::SelectBuild) return false;
  if (state.selectedWorker < 0 || state.selectedWorker > 1) return false;
  if (buildCell < 0 || buildCell >= kCells) return false;

  int from = state.workers[static_cast<int>(state.turn)][state.selectedWorker];
  if (!isAdjacent(from, buildCell)) return false;

  Player owner = Player::None;
  if (workerAt(state, buildCell, owner) != -1) return false;

  if (state.board[buildCell] >= 4) return false;  // already a dome

  return true;
}

bool selectWorker(State& state, int workerIdx) {
  if (state.phase != Phase::SelectWorker && state.phase != Phase::SelectMove) {
    return false;
  }
  if (workerIdx < 0 || workerIdx > 1) return false;
  if (!canWorkerMove(state, state.turn, workerIdx)) return false;

  state.selectedWorker = static_cast<int8_t>(workerIdx);
  state.phase = Phase::SelectMove;
  return true;
}

bool executeMove(State& state, int targetCell) {
  if (state.phase != Phase::SelectMove) return false;
  if (!isLegalMove(state, state.selectedWorker, targetCell)) return false;

  state.workers[static_cast<int>(state.turn)][state.selectedWorker] = static_cast<int8_t>(targetCell);

  // Victory check: worker climbs onto level 3
  if (state.board[targetCell] == 3) {
    state.phase = Phase::GameOver;
    state.winner = state.turn;
    return true;
  }

  state.phase = Phase::SelectBuild;
  return true;
}

bool executeBuild(State& state, int buildCell) {
  if (state.phase != Phase::SelectBuild) return false;
  if (!isLegalBuild(state, buildCell)) return false;

  state.board[buildCell]++;

  Player nextP = opponent(state.turn);
  if (!hasAnyLegalMoves(state, nextP)) {
    // Next player is trapped! Active player wins
    state.phase = Phase::GameOver;
    state.winner = state.turn;
    return true;
  }

  state.turn = nextP;
  state.phase = Phase::SelectWorker;
  state.selectedWorker = -1;
  return true;
}

bool executeFullMove(State& state, const Move& move) {
  if (!selectWorker(state, move.worker)) return false;
  if (!executeMove(state, move.moveCell)) return false;
  if (state.phase == Phase::GameOver) return true;
  return executeBuild(state, move.buildCell);
}

int getLegalMovesForWorker(const State& state, int workerIdx, int8_t* outMoves, int maxMoves) {
  if (workerIdx < 0 || workerIdx > 1) return 0;
  int count = 0;
  int from = state.workers[static_cast<int>(state.turn)][workerIdx];
  int fx = cellX(from);
  int fy = cellY(from);

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      int nx = fx + dx;
      int ny = fy + dy;
      if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
        int target = cellIdx(nx, ny);
        if (isLegalMove(state, workerIdx, target)) {
          if (count < maxMoves) {
            outMoves[count++] = static_cast<int8_t>(target);
          }
        }
      }
    }
  }
  return count;
}

int getLegalBuilds(const State& state, int8_t* outBuilds, int maxBuilds) {
  if (state.phase != Phase::SelectBuild || state.selectedWorker < 0) return 0;
  int count = 0;
  int from = state.workers[static_cast<int>(state.turn)][state.selectedWorker];
  int fx = cellX(from);
  int fy = cellY(from);

  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      if (dx == 0 && dy == 0) continue;
      int nx = fx + dx;
      int ny = fy + dy;
      if (nx >= 0 && nx < kCols && ny >= 0 && ny < kRows) {
        int target = cellIdx(nx, ny);
        if (isLegalBuild(state, target)) {
          if (count < maxBuilds) {
            outBuilds[count++] = static_cast<int8_t>(target);
          }
        }
      }
    }
  }
  return count;
}

}  // namespace santorini
