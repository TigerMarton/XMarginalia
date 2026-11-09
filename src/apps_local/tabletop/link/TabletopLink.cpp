#include "TabletopLink.h"

#include <cstring>

namespace tabletop {
namespace link {

namespace {

uint8_t computeChecksum(const uint8_t* data, const size_t len) {
  uint8_t c = 0xAA;
  for (size_t i = 0; i < len; ++i) {
    c = static_cast<uint8_t>(c ^ data[i]);
  }
  return c;
}

size_t writeEnvelope(uint8_t* out, const size_t capacity, const PacketKind kind, const uint16_t seq,
                     const uint8_t* payload, const uint8_t payloadLen) {
  const size_t total = kHeaderSize + payloadLen + 1;
  if (capacity < total) return 0;

  out[0] = static_cast<uint8_t>(kMagic & 0xFF);
  out[1] = static_cast<uint8_t>((kMagic >> 8) & 0xFF);
  out[2] = static_cast<uint8_t>(kind);
  out[3] = static_cast<uint8_t>(seq & 0xFF);
  out[4] = static_cast<uint8_t>((seq >> 8) & 0xFF);
  out[5] = payloadLen;

  if (payloadLen > 0 && payload != nullptr) {
    std::memcpy(out + kHeaderSize, payload, payloadLen);
  }

  out[total - 1] = computeChecksum(out, total - 1);
  return total;
}

bool readEnvelope(const uint8_t* in, const size_t len, const PacketKind expectedKind, uint16_t& outSeq,
                  uint8_t* outPayload, const uint8_t expectedPayloadLen) {
  const size_t total = kHeaderSize + expectedPayloadLen + 1;
  if (in == nullptr || len < total) return false;

  const uint16_t magic = static_cast<uint16_t>(in[0] | (in[1] << 8));
  if (magic != kMagic) return false;
  if (in[2] != static_cast<uint8_t>(expectedKind)) return false;
  if (in[5] != expectedPayloadLen) return false;

  const uint8_t expectedCrc = in[total - 1];
  if (computeChecksum(in, total - 1) != expectedCrc) return false;

  outSeq = static_cast<uint16_t>(in[3] | (in[4] << 8));
  if (expectedPayloadLen > 0 && outPayload != nullptr) {
    std::memcpy(outPayload, in + kHeaderSize, expectedPayloadLen);
  }
  return true;
}

}  // namespace

size_t encodeReversi(uint8_t* out, const size_t capacity, const uint16_t seq, const ReversiPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::ReversiMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(ReversiPacket));
}

bool decodeReversi(const uint8_t* in, const size_t len, uint16_t& outSeq, ReversiPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::ReversiMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(ReversiPacket))) {
    return false;
  }
  if (outPkt.col >= 8 || outPkt.row >= 8) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeMancala(uint8_t* out, const size_t capacity, const uint16_t seq, const MancalaPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::MancalaMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(MancalaPacket));
}

bool decodeMancala(const uint8_t* in, const size_t len, uint16_t& outSeq, MancalaPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::MancalaMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(MancalaPacket))) {
    return false;
  }
  if (outPkt.pit >= 14) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeMorris(uint8_t* out, const size_t capacity, const uint16_t seq, const MorrisPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::MorrisMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(MorrisPacket));
}

bool decodeMorris(const uint8_t* in, const size_t len, uint16_t& outSeq, MorrisPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::MorrisMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(MorrisPacket))) {
    return false;
  }
  if (outPkt.kind > static_cast<uint8_t>(MorrisActionKind::Remove)) return false;
  if (outPkt.to >= 24) return false;
  if (outPkt.from != 255 && outPkt.from >= 24) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeOnitama(uint8_t* out, const size_t capacity, const uint16_t seq, const OnitamaPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::OnitamaMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(OnitamaPacket));
}

bool decodeOnitama(const uint8_t* in, const size_t len, uint16_t& outSeq, OnitamaPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::OnitamaMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(OnitamaPacket))) {
    return false;
  }
  if (outPkt.from >= 25 || outPkt.to >= 25) return false;
  if (outPkt.cardIdx > 1) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeLiarsDice(uint8_t* out, const size_t capacity, const uint16_t seq, const LiarsDicePacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::LiarsDiceAction, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(LiarsDicePacket));
}

bool decodeLiarsDice(const uint8_t* in, const size_t len, uint16_t& outSeq, LiarsDicePacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::LiarsDiceAction, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(LiarsDicePacket))) {
    return false;
  }
  if (outPkt.kind > static_cast<uint8_t>(LiarsDiceActionKind::NextRound)) return false;
  if (outPkt.face < 1 || outPkt.face > 6) return false;
  return true;
}

size_t encodeDobutsu(uint8_t* out, const size_t capacity, const uint16_t seq, const DobutsuPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::DobutsuMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(DobutsuPacket));
}

bool decodeDobutsu(const uint8_t* in, const size_t len, uint16_t& outSeq, DobutsuPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::DobutsuMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(DobutsuPacket))) {
    return false;
  }
  if (outPkt.to >= 12) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeBackgammon(uint8_t* out, const size_t capacity, const uint16_t seq, const BackgammonPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::BackgammonMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(BackgammonPacket));
}

bool decodeBackgammon(const uint8_t* in, const size_t len, uint16_t& outSeq, BackgammonPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::BackgammonMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(BackgammonPacket))) {
    return false;
  }
  if (outPkt.from < -1 || outPkt.from >= 24) return false;
  if (outPkt.to < -2 || outPkt.to >= 24) return false;
  if (outPkt.die < 1 || outPkt.die > 6) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeSantorini(uint8_t* out, const size_t capacity, const uint16_t seq, const SantoriniPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::SantoriniMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(SantoriniPacket));
}

bool decodeSantorini(const uint8_t* in, const size_t len, uint16_t& outSeq, SantoriniPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::SantoriniMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(SantoriniPacket))) {
    return false;
  }
  if (outPkt.worker < 0 || outPkt.worker > 1) return false;
  if (outPkt.moveCell < 0 || outPkt.moveCell >= 25) return false;
  if (outPkt.buildCell < 0 || outPkt.buildCell >= 25) return false;
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeQuoridor(uint8_t* out, const size_t capacity, const uint16_t seq, const QuoridorPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::QuoridorMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(QuoridorPacket));
}

bool decodeQuoridor(const uint8_t* in, const size_t len, uint16_t& outSeq, QuoridorPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::QuoridorMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(QuoridorPacket))) {
    return false;
  }
  if (outPkt.isWall > 1) return false;
  if (outPkt.isWall == 0) {
    if (outPkt.targetCell < 0 || outPkt.targetCell >= 81) return false;
  } else {
    if (outPkt.wallGx < 0 || outPkt.wallGx >= 8) return false;
    if (outPkt.wallGy < 0 || outPkt.wallGy >= 8) return false;
    if (outPkt.wallOrient > 1) return false;
  }
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeDominoes(uint8_t* out, const size_t capacity, const uint16_t seq, const DominoesPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::DominoesMove, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(DominoesPacket));
}

bool decodeDominoes(const uint8_t* in, const size_t len, uint16_t& outSeq, DominoesPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::DominoesMove, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(DominoesPacket))) {
    return false;
  }
  if (outPkt.action > 3) return false;
  if (outPkt.action <= 1) {
    if (outPkt.handIdx < 0 || outPkt.handIdx >= 28) return false;
  }
  if (outPkt.player < 1 || outPkt.player > 2) return false;
  return true;
}

size_t encodeSandbox(uint8_t* out, const size_t capacity, const uint16_t seq, const SandboxPacket& pkt) {
  return writeEnvelope(out, capacity, PacketKind::SandboxEvent, seq, reinterpret_cast<const uint8_t*>(&pkt),
                       sizeof(SandboxPacket));
}

bool decodeSandbox(const uint8_t* in, const size_t len, uint16_t& outSeq, SandboxPacket& outPkt) {
  if (!readEnvelope(in, len, PacketKind::SandboxEvent, outSeq, reinterpret_cast<uint8_t*>(&outPkt),
                    sizeof(SandboxPacket))) {
    return false;
  }
  if (outPkt.action > static_cast<uint8_t>(SandboxActionKind::ClearTokens)) return false;
  return true;
}

size_t encodeHeartbeat(uint8_t* out, const size_t capacity, const uint16_t seq) {
  return writeEnvelope(out, capacity, PacketKind::Heartbeat, seq, nullptr, 0);
}

bool decodeHeartbeat(const uint8_t* in, const size_t len, uint16_t& outSeq) {
  return readEnvelope(in, len, PacketKind::Heartbeat, outSeq, nullptr, 0);
}

}  // namespace link
}  // namespace tabletop
