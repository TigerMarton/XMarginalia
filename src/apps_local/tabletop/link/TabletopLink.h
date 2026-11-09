#pragma once

#include <cstddef>
#include <cstdint>

namespace tabletop {
namespace link {

constexpr uint16_t kMagic = 0x5454;  // 'T', 'T'
constexpr size_t kHeaderSize = 6;
constexpr size_t kMaxPacketSize = 32;

enum class PacketKind : uint8_t {
  Unknown = 0,
  ReversiMove = 1,
  MancalaMove = 2,
  MorrisMove = 3,
  SandboxEvent = 4,
  Heartbeat = 5,
  OnitamaMove = 6,
  LiarsDiceAction = 7,
  DobutsuMove = 8,
  BackgammonMove = 9,
  SantoriniMove = 10,
  QuoridorMove = 11,
  DominoesMove = 12,
};

enum class LiarsDiceActionKind : uint8_t {
  Bid = 0,
  Challenge = 1,
  NextRound = 2,
};

enum class MorrisActionKind : uint8_t {
  Place = 0,
  Move = 1,
  Remove = 2,
};

enum class SandboxActionKind : uint8_t {
  RollDice = 0,
  DrawCard = 1,
  DiscardCard = 2,
  ScoreChange = 3,
  TokenMove = 4,
  TokenSpawn = 5,
  ClearTokens = 6,
};

#pragma pack(push, 1)

struct Header {
  uint16_t magic;      // 0x5454
  uint8_t kind;        // PacketKind
  uint16_t seq;        // Sequence number
  uint8_t payloadLen;  // Bytes in payload
};

struct ReversiPacket {
  uint8_t col;     // 0..7
  uint8_t row;     // 0..7
  uint8_t player;  // 1 = Black, 2 = White
  uint8_t isPass;  // 1 = pass turn, 0 = move
};

struct MancalaPacket {
  uint8_t pit;     // 0..13
  uint8_t player;  // 1 = South (P1), 2 = North (P2)
};

struct MorrisPacket {
  uint8_t kind;    // MorrisActionKind (Place, Move, Remove)
  uint8_t from;    // 0..23 (255 for place/remove)
  uint8_t to;      // 0..23
  uint8_t player;  // 1 = White, 2 = Black
};

struct OnitamaPacket {
  uint8_t from;     // 0..24
  uint8_t to;       // 0..24
  uint8_t cardIdx;  // 0 or 1
  uint8_t player;   // 1 = P1, 2 = P2
};

struct LiarsDicePacket {
  uint8_t kind;    // LiarsDiceActionKind
  uint8_t count;   // 1..20
  uint8_t face;    // 1..6
  uint8_t player;  // player index
};

struct DobutsuPacket {
  int8_t from;      // 0..11 or kDrop (-1, -2, -3)
  uint8_t to;       // 0..11
  uint8_t player;   // 1 = P1, 2 = P2
  uint8_t promotes; // 1 = true, 0 = false
};

struct BackgammonPacket {
  int8_t from;     // 0..23, or -1 (Bar)
  int8_t to;       // 0..23, or -2 (Off)
  uint8_t die;     // 1..6
  uint8_t player;  // 1 = White, 2 = Black
};

struct SantoriniPacket {
  int8_t worker;     // 0 or 1
  int8_t moveCell;   // 0..24
  int8_t buildCell;  // 0..24
  uint8_t player;    // 1 = White, 2 = Black
};

struct QuoridorPacket {
  uint8_t isWall;      // 0 = pawn move, 1 = wall placement
  int8_t targetCell;   // 0..80 (for pawn move)
  int8_t wallGx;       // 0..7
  int8_t wallGy;       // 0..7
  uint8_t wallOrient;  // 0 = Horizontal, 1 = Vertical
  uint8_t player;      // 1 = White, 2 = Black
};

struct DominoesPacket {
  uint8_t action;  // 0 = PlayLeft, 1 = PlayRight, 2 = Draw, 3 = Pass
  int8_t handIdx;  // 0..27 (for play)
  uint8_t player;  // 1 = P1, 2 = P2
};

struct SandboxPacket {
  uint8_t action;  // SandboxActionKind
  uint8_t itemId;  // die idx, card idx, player idx, token idx
  uint8_t val8;    // die type, card, color, etc.
  int16_t val16;   // delta, token pos, etc.
};

#pragma pack(pop)

// Encode functions return bytes written, or 0 on error / buffer too small.
size_t encodeReversi(uint8_t* out, size_t capacity, uint16_t seq, const ReversiPacket& pkt);
bool decodeReversi(const uint8_t* in, size_t len, uint16_t& outSeq, ReversiPacket& outPkt);

size_t encodeMancala(uint8_t* out, size_t capacity, uint16_t seq, const MancalaPacket& pkt);
bool decodeMancala(const uint8_t* in, size_t len, uint16_t& outSeq, MancalaPacket& outPkt);

size_t encodeMorris(uint8_t* out, size_t capacity, uint16_t seq, const MorrisPacket& pkt);
bool decodeMorris(const uint8_t* in, size_t len, uint16_t& outSeq, MorrisPacket& outPkt);

size_t encodeOnitama(uint8_t* out, size_t capacity, uint16_t seq, const OnitamaPacket& pkt);
bool decodeOnitama(const uint8_t* in, size_t len, uint16_t& outSeq, OnitamaPacket& outPkt);

size_t encodeLiarsDice(uint8_t* out, size_t capacity, uint16_t seq, const LiarsDicePacket& pkt);
bool decodeLiarsDice(const uint8_t* in, size_t len, uint16_t& outSeq, LiarsDicePacket& outPkt);

size_t encodeDobutsu(uint8_t* out, size_t capacity, uint16_t seq, const DobutsuPacket& pkt);
bool decodeDobutsu(const uint8_t* in, size_t len, uint16_t& outSeq, DobutsuPacket& outPkt);

size_t encodeBackgammon(uint8_t* out, size_t capacity, uint16_t seq, const BackgammonPacket& pkt);
bool decodeBackgammon(const uint8_t* in, size_t len, uint16_t& outSeq, BackgammonPacket& outPkt);

size_t encodeSantorini(uint8_t* out, size_t capacity, uint16_t seq, const SantoriniPacket& pkt);
bool decodeSantorini(const uint8_t* in, size_t len, uint16_t& outSeq, SantoriniPacket& outPkt);

size_t encodeQuoridor(uint8_t* out, size_t capacity, uint16_t seq, const QuoridorPacket& pkt);
bool decodeQuoridor(const uint8_t* in, size_t len, uint16_t& outSeq, QuoridorPacket& outPkt);

size_t encodeDominoes(uint8_t* out, size_t capacity, uint16_t seq, const DominoesPacket& pkt);
bool decodeDominoes(const uint8_t* in, size_t len, uint16_t& outSeq, DominoesPacket& outPkt);

size_t encodeSandbox(uint8_t* out, size_t capacity, uint16_t seq, const SandboxPacket& pkt);
bool decodeSandbox(const uint8_t* in, size_t len, uint16_t& outSeq, SandboxPacket& outPkt);

size_t encodeHeartbeat(uint8_t* out, size_t capacity, uint16_t seq);
bool decodeHeartbeat(const uint8_t* in, size_t len, uint16_t& outSeq);

}  // namespace link
}  // namespace tabletop
