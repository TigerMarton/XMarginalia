#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "NearbyTransfer.h"

namespace nearby = freeink::nearby;

static int testsRun = 0;
#define TEST_ASSERT(cond)                                                \
  do {                                                                   \
    ++testsRun;                                                          \
    if (!(cond)) {                                                       \
      fprintf(stderr, "FAIL: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
      exit(1);                                                           \
    }                                                                    \
  } while (0)

// CRC32 test vector for "123456789": standard CRC-32 is 0xCBF43926.
// Note that nearby::crc32Update computes the inverted register state,
// and ReliableTransferSession::crc32() post-inverts it (^ 0xFFFFFFFFu).
void testCrc32() {
  const char* sample = "123456789";
  const size_t len = strlen(sample);

  uint32_t rawCrc = 0xFFFFFFFFu;
  rawCrc = nearby::crc32Update(rawCrc, reinterpret_cast<const uint8_t*>(sample), len);
  const uint32_t finalCrc = rawCrc ^ 0xFFFFFFFFu;
  TEST_ASSERT(finalCrc == 0xCBF43926u);

  // Incremental update produces the exact same result.
  uint32_t splitCrc = 0xFFFFFFFFu;
  splitCrc = nearby::crc32Update(splitCrc, reinterpret_cast<const uint8_t*>(sample), 4);
  splitCrc = nearby::crc32Update(splitCrc, reinterpret_cast<const uint8_t*>(sample + 4), len - 4);
  TEST_ASSERT((splitCrc ^ 0xFFFFFFFFu) == 0xCBF43926u);
}

void testIntegerSerialization() {
  uint8_t buf[8] = {0};

  nearby::writeU16(buf, 0x1234);
  TEST_ASSERT(nearby::readU16(buf) == 0x1234);

  nearby::writeU32(buf, 0x89ABCDEFu);
  TEST_ASSERT(nearby::readU32(buf) == 0x89ABCDEFu);

  const uint64_t largeVal = 0x1122334455667788ULL;
  nearby::writeU64(buf, largeVal);
  TEST_ASSERT(nearby::readU64(buf) == largeVal);
}

void testPacketEncodingDecoding() {
  uint8_t buffer[nearby::MAX_PACKET_BYTES] = {0};
  size_t length = 0;

  const uint32_t sessionId = 0xA1B2C3D4;
  const uint32_t sequence = 42;
  const char payloadData[] = "CrossInk/CrossPoint File Transfer Test Payload";
  const uint16_t payloadLen = static_cast<uint16_t>(strlen(payloadData));

  // Encode Data packet
  const bool encOk = nearby::encodePacket(buffer, sizeof(buffer), nearby::PacketType::Data, sessionId, sequence,
                                          payloadData, payloadLen, length);
  TEST_ASSERT(encOk);
  TEST_ASSERT(length == nearby::PACKET_HEADER_BYTES + payloadLen);

  // Decode Data packet
  nearby::PacketView view;
  const bool decOk = nearby::decodePacket(buffer, length, view);
  TEST_ASSERT(decOk);
  TEST_ASSERT(view.type == nearby::PacketType::Data);
  TEST_ASSERT(view.sessionId == sessionId);
  TEST_ASSERT(view.sequence == sequence);
  TEST_ASSERT(view.payloadLength == payloadLen);
  TEST_ASSERT(memcmp(view.payload, payloadData, payloadLen) == 0);

  // Malformed packets: truncated header
  nearby::PacketView badView;
  TEST_ASSERT(!nearby::decodePacket(buffer, nearby::PACKET_HEADER_BYTES - 1, badView));

  // Malformed packets: bad magic
  buffer[0] = 'X';
  TEST_ASSERT(!nearby::decodePacket(buffer, length, badView));
  buffer[0] = 'C';

  // Malformed packets: bad protocol version
  buffer[4] = 99;
  TEST_ASSERT(!nearby::decodePacket(buffer, length, badView));
  buffer[4] = nearby::PROTOCOL_VERSION;

  // Empty payload packet (e.g. Cancel)
  const bool encCancel =
      nearby::encodePacket(buffer, sizeof(buffer), nearby::PacketType::Cancel, sessionId, 0, nullptr, 0, length);
  TEST_ASSERT(encCancel);
  TEST_ASSERT(length == nearby::PACKET_HEADER_BYTES);

  const bool decCancel = nearby::decodePacket(buffer, length, view);
  TEST_ASSERT(decCancel);
  TEST_ASSERT(view.type == nearby::PacketType::Cancel);
  TEST_ASSERT(view.payloadLength == 0);
}

void testReliableTransferSessionFlow() {
  nearby::ReliableTransferSession sender;
  nearby::ReliableTransferSession receiver;

  const uint32_t sessionId = 0x55AA1122;
  const uint64_t totalBytes = 2500;
  const uint16_t chunkSize = 1024;

  sender.begin(nearby::ReliableTransferSession::Role::Sender, sessionId, totalBytes, chunkSize);
  receiver.begin(nearby::ReliableTransferSession::Role::Receiver, sessionId, totalBytes, chunkSize);

  TEST_ASSERT(sender.role() == nearby::ReliableTransferSession::Role::Sender);
  TEST_ASSERT(receiver.role() == nearby::ReliableTransferSession::Role::Receiver);
  TEST_ASSERT(sender.totalBytes() == totalBytes);
  TEST_ASSERT(receiver.totalBytes() == totalBytes);
  TEST_ASSERT(sender.nextSequence() == 0);
  TEST_ASSERT(receiver.nextSequence() == 0);

  // Simulate chunk 0: 1024 bytes
  std::vector<uint8_t> chunk0(1024, 0x41);
  sender.includeBytes(chunk0.data(), chunk0.size());
  // Receiver accepts chunk 0
  TEST_ASSERT(receiver.acceptReceivedChunk(0, chunk0.size()));
  receiver.includeBytes(chunk0.data(), chunk0.size());
  TEST_ASSERT(receiver.nextSequence() == 1);
  TEST_ASSERT(receiver.transferredBytes() == 1024);

  // Sender receives Ack with nextSequence = 1
  TEST_ASSERT(sender.acceptAcknowledgement(1));
  sender.advanceSentBytes(chunk0.size());
  TEST_ASSERT(sender.nextSequence() == 1);
  TEST_ASSERT(sender.transferredBytes() == 1024);

  // Stale Ack must be rejected
  TEST_ASSERT(!sender.acceptAcknowledgement(1));

  // Chunk 1: 1024 bytes
  std::vector<uint8_t> chunk1(1024, 0x42);
  sender.includeBytes(chunk1.data(), chunk1.size());
  // Out of order chunk (sequence 3 instead of 1) must be rejected
  TEST_ASSERT(!receiver.acceptReceivedChunk(3, chunk1.size()));
  // Correct sequence
  TEST_ASSERT(receiver.acceptReceivedChunk(1, chunk1.size()));
  receiver.includeBytes(chunk1.data(), chunk1.size());
  TEST_ASSERT(sender.acceptAcknowledgement(2));
  sender.advanceSentBytes(chunk1.size());

  // Chunk 2: remaining 452 bytes (2500 - 2048)
  std::vector<uint8_t> chunk2(452, 0x43);
  sender.includeBytes(chunk2.data(), chunk2.size());
  TEST_ASSERT(receiver.acceptReceivedChunk(2, chunk2.size()));
  receiver.includeBytes(chunk2.data(), chunk2.size());
  TEST_ASSERT(sender.acceptAcknowledgement(3));
  sender.advanceSentBytes(chunk2.size());

  // Check CRC and byte completion
  TEST_ASSERT(sender.transferredBytes() == totalBytes);
  TEST_ASSERT(receiver.transferredBytes() == totalBytes);
  TEST_ASSERT(sender.crc32() == receiver.crc32());

  // Overflow attempt: Receiver rejecting bytes beyond totalBytes
  std::vector<uint8_t> extra(10, 0xFF);
  TEST_ASSERT(!receiver.acceptReceivedChunk(3, extra.size()));
}

void testWirePayloadEncodingCrossInk() {
  // Verify Offer payload:
  // 12 bytes header (u64 size, u16 chunk, u8 senderLen, u8 nameLen) + senderName + fileName
  const uint64_t fileSize = 1500000;
  const uint16_t chunkBytes = 1024;
  const std::string senderName = "David's Picco";
  const std::string fileName = "Moby Dick.epub";

  uint8_t offerPayload[256] = {0};
  nearby::writeU64(offerPayload, fileSize);
  nearby::writeU16(offerPayload + 8, chunkBytes);
  offerPayload[10] = static_cast<uint8_t>(senderName.size());
  offerPayload[11] = static_cast<uint8_t>(fileName.size());
  memcpy(offerPayload + 12, senderName.data(), senderName.size());
  memcpy(offerPayload + 12 + senderName.size(), fileName.data(), fileName.size());

  const size_t totalOfferLen = 12 + senderName.size() + fileName.size();

  // Parse Offer payload on receiver side
  TEST_ASSERT(nearby::readU64(offerPayload) == fileSize);
  TEST_ASSERT(nearby::readU16(offerPayload + 8) == chunkBytes);
  const uint8_t parsedSenderLen = offerPayload[10];
  const uint8_t parsedNameLen = offerPayload[11];
  TEST_ASSERT(parsedSenderLen == senderName.size());
  TEST_ASSERT(parsedNameLen == fileName.size());
  TEST_ASSERT(totalOfferLen == 12u + parsedSenderLen + parsedNameLen);

  std::string parsedSender(reinterpret_cast<const char*>(offerPayload + 12), parsedSenderLen);
  std::string parsedName(reinterpret_cast<const char*>(offerPayload + 12 + parsedSenderLen), parsedNameLen);
  TEST_ASSERT(parsedSender == senderName);
  TEST_ASSERT(parsedName == fileName);

  // Verify Complete payload:
  // 12 bytes: u64 totalBytes + u32 crc32
  const uint32_t expectedCrc = 0x12345678;
  uint8_t completePayload[12] = {0};
  nearby::writeU64(completePayload, fileSize);
  nearby::writeU32(completePayload + 8, expectedCrc);

  TEST_ASSERT(nearby::readU64(completePayload) == fileSize);
  TEST_ASSERT(nearby::readU32(completePayload + 8) == expectedCrc);

  // Verify Ack payload:
  // 4 bytes: u32 nextSequence
  uint8_t ackPayload[4] = {0};
  nearby::writeU32(ackPayload, 55);
  TEST_ASSERT(nearby::readU32(ackPayload) == 55);
}

int main() {
  printf("Running Nearby Transfer Protocol host unit tests...\n");
  testCrc32();
  testIntegerSerialization();
  testPacketEncodingDecoding();
  testReliableTransferSessionFlow();
  testWirePayloadEncodingCrossInk();
  printf("All %d Nearby Transfer checks passed successfully!\n", testsRun);
  return 0;
}
