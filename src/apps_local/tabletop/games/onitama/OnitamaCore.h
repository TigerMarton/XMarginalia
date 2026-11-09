#pragma once

#include <cstddef>
#include <cstdint>

namespace onitama {

constexpr int kBoardWidth = 5;
constexpr int kBoardHeight = 5;
constexpr int kBoardCells = 25;

constexpr uint8_t kEmpty = 0;
constexpr uint8_t kP1Student = 1;
constexpr uint8_t kP1Master = 2;
constexpr uint8_t kP2Student = 3;
constexpr uint8_t kP2Master = 4;

constexpr uint8_t kPlayer1 = 1;
constexpr uint8_t kPlayer2 = 2;

constexpr int kP1Temple = 22;  // (x=2, y=4) -> 4 * 5 + 2
constexpr int kP2Temple = 2;   // (x=2, y=0) -> 0 * 5 + 2

enum class WinReason : uint8_t {
  None = 0,
  Stone = 1,   // Captured enemy Master
  Stream = 2,  // Reached enemy Temple arch with own Master
};

struct CardDef {
  const char* name;
  int moveCount;
  // Up to 4 relative offsets (dx, dy) from Player 1's perspective.
  // dy > 0: forward towards P2.
  // dy < 0: backward away from P2.
  int8_t moves[4][2];
};

extern const CardDef kCards[16];

struct Move {
  uint8_t from;     // 0..24
  uint8_t to;       // 0..24
  uint8_t cardIdx;  // 0 or 1: index into active player's cards array
};

struct Game {
  uint8_t cells[kBoardCells];
  uint8_t p1Cards[2];  // Indices 0..15 into kCards
  uint8_t p2Cards[2];
  uint8_t tableCard;
  uint8_t turn;  // kPlayer1 or kPlayer2
  bool isOver;
  uint8_t winner;  // 0=none, 1=P1, 2=P2
  WinReason winReason;
};

inline bool isP1(const uint8_t piece) {
  return piece == kP1Student || piece == kP1Master;
}

inline bool isP2(const uint8_t piece) {
  return piece == kP2Student || piece == kP2Master;
}

inline bool isMaster(const uint8_t piece) {
  return piece == kP1Master || piece == kP2Master;
}

Game initGame(uint32_t seed = 0);
int legalMoves(const Game& game, Move* outMoves, int maxMoves);
bool isLegalMove(const Game& game, const Move& move);
bool applyMove(Game& game, const Move& move);

}  // namespace onitama
