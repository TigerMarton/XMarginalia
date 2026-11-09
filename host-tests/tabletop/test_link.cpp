#include <cassert>
#include <cstdio>
#include <cstring>

#include "../../src/apps_local/tabletop/link/TabletopLink.h"

using namespace tabletop::link;

void test_reversi_packet() {
  uint8_t buffer[64] = {};
  ReversiPacket pkt{3, 4, 1, 0};
  const size_t encoded = encodeReversi(buffer, sizeof(buffer), 42, pkt);
  assert(encoded == kHeaderSize + sizeof(ReversiPacket) + 1);

  uint16_t seq = 0;
  ReversiPacket outPkt{};
  bool ok = decodeReversi(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 42);
  assert(outPkt.col == 3);
  assert(outPkt.row == 4);
  assert(outPkt.player == 1);
  assert(outPkt.isPass == 0);

  // Corrupted checksum
  buffer[encoded - 1] ^= 0xFF;
  assert(!decodeReversi(buffer, encoded, seq, outPkt));
  buffer[encoded - 1] ^= 0xFF;

  // Out of bounds col
  pkt.col = 8;
  encodeReversi(buffer, sizeof(buffer), 43, pkt);
  assert(!decodeReversi(buffer, encoded, seq, outPkt));

  // Truncated buffer
  assert(!decodeReversi(buffer, encoded - 1, seq, outPkt));

  printf("test_reversi_packet passed\n");
}

void test_mancala_packet() {
  uint8_t buffer[64] = {};
  MancalaPacket pkt{4, 1};
  const size_t encoded = encodeMancala(buffer, sizeof(buffer), 101, pkt);
  assert(encoded == kHeaderSize + sizeof(MancalaPacket) + 1);

  uint16_t seq = 0;
  MancalaPacket outPkt{};
  bool ok = decodeMancala(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 101);
  assert(outPkt.pit == 4);
  assert(outPkt.player == 1);

  // Out of bounds pit
  pkt.pit = 14;
  encodeMancala(buffer, sizeof(buffer), 102, pkt);
  assert(!decodeMancala(buffer, encoded, seq, outPkt));

  printf("test_mancala_packet passed\n");
}

void test_morris_packet() {
  uint8_t buffer[64] = {};
  MorrisPacket pkt{static_cast<uint8_t>(MorrisActionKind::Move), 1, 9, 2};
  const size_t encoded = encodeMorris(buffer, sizeof(buffer), 77, pkt);
  assert(encoded == kHeaderSize + sizeof(MorrisPacket) + 1);

  uint16_t seq = 0;
  MorrisPacket outPkt{};
  bool ok = decodeMorris(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 77);
  assert(outPkt.kind == static_cast<uint8_t>(MorrisActionKind::Move));
  assert(outPkt.from == 1);
  assert(outPkt.to == 9);
  assert(outPkt.player == 2);

  // Out of bounds vertex
  pkt.to = 24;
  encodeMorris(buffer, sizeof(buffer), 78, pkt);
  assert(!decodeMorris(buffer, encoded, seq, outPkt));

  printf("test_morris_packet passed\n");
}

void test_onitama_packet() {
  uint8_t buffer[64] = {};
  OnitamaPacket pkt{12, 7, 0, 1};
  const size_t encoded = encodeOnitama(buffer, sizeof(buffer), 88, pkt);
  assert(encoded == kHeaderSize + sizeof(OnitamaPacket) + 1);

  uint16_t seq = 0;
  OnitamaPacket outPkt{};
  bool ok = decodeOnitama(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 88);
  assert(outPkt.from == 12);
  assert(outPkt.to == 7);
  assert(outPkt.cardIdx == 0);
  assert(outPkt.player == 1);

  // Out of bounds
  pkt.to = 25;
  encodeOnitama(buffer, sizeof(buffer), 89, pkt);
  assert(!decodeOnitama(buffer, encoded, seq, outPkt));

  printf("test_onitama_packet passed\n");
}

void test_liarsdice_packet() {
  uint8_t buffer[64] = {};
  LiarsDicePacket pkt{static_cast<uint8_t>(LiarsDiceActionKind::Bid), 4, 3, 0};
  const size_t encoded = encodeLiarsDice(buffer, sizeof(buffer), 55, pkt);
  assert(encoded == kHeaderSize + sizeof(LiarsDicePacket) + 1);

  uint16_t seq = 0;
  LiarsDicePacket outPkt{};
  bool ok = decodeLiarsDice(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 55);
  assert(outPkt.kind == static_cast<uint8_t>(LiarsDiceActionKind::Bid));
  assert(outPkt.count == 4);
  assert(outPkt.face == 3);
  assert(outPkt.player == 0);

  // Invalid face
  pkt.face = 7;
  encodeLiarsDice(buffer, sizeof(buffer), 56, pkt);
  assert(!decodeLiarsDice(buffer, encoded, seq, outPkt));

  printf("test_liarsdice_packet passed\n");
}

void test_dobutsu_packet() {
  uint8_t buffer[64] = {};
  DobutsuPacket pkt{4, 1, 1, 0};
  const size_t encoded = encodeDobutsu(buffer, sizeof(buffer), 123, pkt);
  assert(encoded == kHeaderSize + sizeof(DobutsuPacket) + 1);

  uint16_t seq = 0;
  DobutsuPacket outPkt{};
  bool ok = decodeDobutsu(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 123);
  assert(outPkt.from == 4);
  assert(outPkt.to == 1);
  assert(outPkt.player == 1);
  assert(outPkt.promotes == 0);

  // Invalid dest
  pkt.to = 15;
  encodeDobutsu(buffer, sizeof(buffer), 124, pkt);
  assert(!decodeDobutsu(buffer, encoded, seq, outPkt));

  printf("test_dobutsu_packet passed\n");
}

void test_backgammon_packet() {
  uint8_t buffer[64] = {};
  BackgammonPacket pkt{13, 10, 3, 1};
  const size_t encoded = encodeBackgammon(buffer, sizeof(buffer), 200, pkt);
  assert(encoded == kHeaderSize + sizeof(BackgammonPacket) + 1);

  uint16_t seq = 0;
  BackgammonPacket outPkt{};
  bool ok = decodeBackgammon(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 200);
  assert(outPkt.from == 13);
  assert(outPkt.to == 10);
  assert(outPkt.die == 3);
  assert(outPkt.player == 1);

  // Bar move
  pkt.from = -1;
  pkt.to = 3;
  encodeBackgammon(buffer, sizeof(buffer), 201, pkt);
  assert(decodeBackgammon(buffer, encoded, seq, outPkt));
  assert(outPkt.from == -1);

  // Bear off move
  pkt.from = 2;
  pkt.to = -2;
  encodeBackgammon(buffer, sizeof(buffer), 202, pkt);
  assert(decodeBackgammon(buffer, encoded, seq, outPkt));
  assert(outPkt.to == -2);

  // Invalid die
  pkt.die = 7;
  encodeBackgammon(buffer, sizeof(buffer), 203, pkt);
  assert(!decodeBackgammon(buffer, encoded, seq, outPkt));

  printf("test_backgammon_packet passed\n");
}

void test_sandbox_packet() {
  uint8_t buffer[64] = {};
  SandboxPacket pkt{static_cast<uint8_t>(SandboxActionKind::TokenMove), 5, 0, 42};
  const size_t encoded = encodeSandbox(buffer, sizeof(buffer), 999, pkt);
  assert(encoded == kHeaderSize + sizeof(SandboxPacket) + 1);

  uint16_t seq = 0;
  SandboxPacket outPkt{};
  bool ok = decodeSandbox(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 999);
  assert(outPkt.action == static_cast<uint8_t>(SandboxActionKind::TokenMove));
  assert(outPkt.itemId == 5);
  assert(outPkt.val16 == 42);

  printf("test_sandbox_packet passed\n");
}

void test_santorini_packet() {
  uint8_t buffer[64] = {};
  SantoriniPacket pkt{0, 12, 13, 1};
  const size_t encoded = encodeSantorini(buffer, sizeof(buffer), 301, pkt);
  assert(encoded == kHeaderSize + sizeof(SantoriniPacket) + 1);

  uint16_t seq = 0;
  SantoriniPacket outPkt{};
  bool ok = decodeSantorini(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 301);
  assert(outPkt.worker == 0);
  assert(outPkt.moveCell == 12);
  assert(outPkt.buildCell == 13);
  assert(outPkt.player == 1);

  // Out of bounds cell
  pkt.moveCell = 25;
  encodeSantorini(buffer, sizeof(buffer), 302, pkt);
  assert(!decodeSantorini(buffer, encoded, seq, outPkt));

  printf("test_santorini_packet passed\n");
}

void test_quoridor_packet() {
  uint8_t buffer[64] = {};
  QuoridorPacket pkt{1, 0, 3, 4, 1, 2};
  const size_t encoded = encodeQuoridor(buffer, sizeof(buffer), 401, pkt);
  assert(encoded == kHeaderSize + sizeof(QuoridorPacket) + 1);

  uint16_t seq = 0;
  QuoridorPacket outPkt{};
  bool ok = decodeQuoridor(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 401);
  assert(outPkt.isWall == 1);
  assert(outPkt.wallGx == 3);
  assert(outPkt.wallGy == 4);
  assert(outPkt.wallOrient == 1);
  assert(outPkt.player == 2);

  // Out of bounds groove
  pkt.wallGx = 8;
  encodeQuoridor(buffer, sizeof(buffer), 402, pkt);
  assert(!decodeQuoridor(buffer, encoded, seq, outPkt));

  printf("test_quoridor_packet passed\n");
}

void test_dominoes_packet() {
  uint8_t buffer[64] = {};
  DominoesPacket pkt{0, 5, 1};
  const size_t encoded = encodeDominoes(buffer, sizeof(buffer), 501, pkt);
  assert(encoded == kHeaderSize + sizeof(DominoesPacket) + 1);

  uint16_t seq = 0;
  DominoesPacket outPkt{};
  bool ok = decodeDominoes(buffer, encoded, seq, outPkt);
  assert(ok);
  assert(seq == 501);
  assert(outPkt.action == 0);
  assert(outPkt.handIdx == 5);
  assert(outPkt.player == 1);

  // Out of bounds handIdx
  pkt.handIdx = 28;
  encodeDominoes(buffer, sizeof(buffer), 502, pkt);
  assert(!decodeDominoes(buffer, encoded, seq, outPkt));

  printf("test_dominoes_packet passed\n");
}

void test_heartbeat() {
  uint8_t buffer[32] = {};
  const size_t encoded = encodeHeartbeat(buffer, sizeof(buffer), 500);
  assert(encoded == kHeaderSize + 1);

  uint16_t seq = 0;
  bool ok = decodeHeartbeat(buffer, encoded, seq);
  assert(ok);
  assert(seq == 500);

  // Insufficient capacity
  assert(encodeHeartbeat(buffer, 4, 501) == 0);

  printf("test_heartbeat passed\n");
}

int main() {
  test_reversi_packet();
  test_mancala_packet();
  test_morris_packet();
  test_onitama_packet();
  test_liarsdice_packet();
  test_dobutsu_packet();
  test_backgammon_packet();
  test_santorini_packet();
  test_quoridor_packet();
  test_dominoes_packet();
  test_sandbox_packet();
  test_heartbeat();
  printf("test_link: all tests passed\n");
  return 0;
}
