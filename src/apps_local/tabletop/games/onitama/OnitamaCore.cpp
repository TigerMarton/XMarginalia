#include "OnitamaCore.h"

#include <algorithm>
#include <cstring>

namespace onitama {

// 16 authentic Onitama movement cards:
// (dx, dy) from Player 1's perspective:
// dx > 0 = right, dx < 0 = left
// dy > 0 = forward (upwards), dy < 0 = backward (downwards)
const CardDef kCards[16] = {
    {"TIGER", 2, {{0, 2}, {0, -1}, {0, 0}, {0, 0}}},
    {"DRAGON", 4, {{-2, 1}, {2, 1}, {-1, -1}, {1, -1}}},
    {"FROG", 3, {{-2, 0}, {-1, 1}, {1, -1}, {0, 0}}},
    {"RABBIT", 3, {{2, 0}, {1, 1}, {-1, -1}, {0, 0}}},
    {"CRAB", 3, {{0, 1}, {-2, 0}, {2, 0}, {0, 0}}},
    {"ELEPHANT", 4, {{-1, 1}, {1, 1}, {-1, 0}, {1, 0}}},
    {"GOOSE", 4, {{-1, 1}, {-1, 0}, {1, 0}, {1, -1}}},
    {"ROOSTER", 4, {{1, 1}, {1, 0}, {-1, 0}, {-1, -1}}},
    {"MONKEY", 4, {{-1, 1}, {1, 1}, {-1, -1}, {1, -1}}},
    {"MANTIS", 3, {{-1, 1}, {1, 1}, {0, -1}, {0, 0}}},
    {"HORSE", 3, {{0, 1}, {-1, 0}, {0, -1}, {0, 0}}},
    {"OX", 3, {{0, 1}, {1, 0}, {0, -1}, {0, 0}}},
    {"CRANE", 3, {{0, 1}, {-1, -1}, {1, -1}, {0, 0}}},
    {"BOAR", 3, {{0, 1}, {-1, 0}, {1, 0}, {0, 0}}},
    {"EEL", 3, {{-1, 1}, {1, 0}, {-1, -1}, {0, 0}}},
    {"COBRA", 3, {{1, 1}, {-1, 0}, {1, -1}, {0, 0}}},
};

Game initGame(uint32_t seed) {
  Game g = {};
  std::memset(g.cells, kEmpty, sizeof(g.cells));

  // Player 2 starting rank (Row 0)
  g.cells[0] = kP2Student;
  g.cells[1] = kP2Student;
  g.cells[2] = kP2Master;
  g.cells[3] = kP2Student;
  g.cells[4] = kP2Student;

  // Player 1 starting rank (Row 4)
  g.cells[20] = kP1Student;
  g.cells[21] = kP1Student;
  g.cells[22] = kP1Master;
  g.cells[23] = kP1Student;
  g.cells[24] = kP1Student;

  // Select 5 distinct cards
  uint8_t pool[16];
  for (uint8_t i = 0; i < 16; ++i) {
    pool[i] = i;
  }

  // Simple LCG shuffle
  uint32_t rng = seed != 0 ? seed : 0x12345678;
  for (int i = 15; i > 0; --i) {
    rng = rng * 1664525u + 1013904223u;
    const int j = static_cast<int>((rng >> 16) % static_cast<uint32_t>(i + 1));
    const uint8_t tmp = pool[i];
    pool[i] = pool[j];
    pool[j] = tmp;
  }

  g.p1Cards[0] = pool[0];
  g.p1Cards[1] = pool[1];
  g.p2Cards[0] = pool[2];
  g.p2Cards[1] = pool[3];
  g.tableCard = pool[4];

  g.turn = kPlayer1;
  g.isOver = false;
  g.winner = 0;
  g.winReason = WinReason::None;
  return g;
}

int legalMoves(const Game& game, Move* outMoves, const int maxMoves) {
  if (game.isOver || !outMoves || maxMoves <= 0) return 0;

  int count = 0;
  const uint8_t active = game.turn;
  const uint8_t* myCards = (active == kPlayer1) ? game.p1Cards : game.p2Cards;

  for (uint8_t from = 0; from < kBoardCells; ++from) {
    const uint8_t piece = game.cells[from];
    if (active == kPlayer1 && !isP1(piece)) continue;
    if (active == kPlayer2 && !isP2(piece)) continue;

    const int fromX = from % kBoardWidth;
    const int fromY = from / kBoardWidth;

    for (uint8_t cIdx = 0; cIdx < 2; ++cIdx) {
      const CardDef& card = kCards[myCards[cIdx]];
      for (int m = 0; m < card.moveCount; ++m) {
        int dx = card.moves[m][0];
        int dy = card.moves[m][1];

        int toX = 0;
        int toY = 0;
        if (active == kPlayer1) {
          toX = fromX + dx;
          toY = fromY - dy;  // dy > 0 is forward towards row 0
        } else {
          toX = fromX - dx;  // 180° inverted for P2
          toY = fromY + dy;  // dy > 0 is forward towards row 4
        }

        if (toX < 0 || toX >= kBoardWidth || toY < 0 || toY >= kBoardHeight) {
          continue;
        }

        const uint8_t to = static_cast<uint8_t>(toY * kBoardWidth + toX);
        const uint8_t destPiece = game.cells[to];

        // Cannot capture friendly pieces
        if (active == kPlayer1 && isP1(destPiece)) continue;
        if (active == kPlayer2 && isP2(destPiece)) continue;

        if (count < maxMoves) {
          outMoves[count].from = from;
          outMoves[count].to = to;
          outMoves[count].cardIdx = cIdx;
          ++count;
        }
      }
    }
  }

  return count;
}

bool isLegalMove(const Game& game, const Move& move) {
  if (game.isOver) return false;
  if (move.from >= kBoardCells || move.to >= kBoardCells || move.cardIdx > 1) return false;

  Move list[64];
  const int n = legalMoves(game, list, 64);
  for (int i = 0; i < n; ++i) {
    if (list[i].from == move.from && list[i].to == move.to && list[i].cardIdx == move.cardIdx) {
      return true;
    }
  }
  return false;
}

bool applyMove(Game& game, const Move& move) {
  if (!isLegalMove(game, move)) return false;

  const uint8_t piece = game.cells[move.from];
  const uint8_t dest = game.cells[move.to];
  const uint8_t active = game.turn;

  // Move piece
  game.cells[move.from] = kEmpty;
  game.cells[move.to] = piece;

  // Check Stone win condition (captured enemy Master)
  if (dest == kP2Master) {
    game.isOver = true;
    game.winner = kPlayer1;
    game.winReason = WinReason::Stone;
  } else if (dest == kP1Master) {
    game.isOver = true;
    game.winner = kPlayer2;
    game.winReason = WinReason::Stone;
  }

  // Check Stream win condition (friendly Master reaches opponent's Temple arch)
  if (!game.isOver) {
    if (piece == kP1Master && move.to == kP2Temple) {
      game.isOver = true;
      game.winner = kPlayer1;
      game.winReason = WinReason::Stream;
    } else if (piece == kP2Master && move.to == kP1Temple) {
      game.isOver = true;
      game.winner = kPlayer2;
      game.winReason = WinReason::Stream;
    }
  }

  // Exchange played card with tableCard
  if (active == kPlayer1) {
    const uint8_t used = game.p1Cards[move.cardIdx];
    game.p1Cards[move.cardIdx] = game.tableCard;
    game.tableCard = used;
    game.turn = kPlayer2;
  } else {
    const uint8_t used = game.p2Cards[move.cardIdx];
    game.p2Cards[move.cardIdx] = game.tableCard;
    game.tableCard = used;
    game.turn = kPlayer1;
  }

  return true;
}

}  // namespace onitama
