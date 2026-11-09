#include "NearbyBookTransferActivity.h"

#include <Arduino.h>
#include <FontCacheManager.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "DevMode.h"
#include "MappedInputManager.h"
#include "apps_local/link/LinkRadio.h"
#include "apps_local/player/PlayerName.h"
#include "components/UITheme.h"
#include "fontIds.h"

#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
#include <WiFi.h>
#include <esp_system.h>
#endif

namespace nearby = freeink::nearby;
namespace fui = freeink::ui;

namespace {
constexpr const char* LOG_TAG = "NBOOK";
constexpr uint8_t BROADCAST_MAC[nearby::MAC_BYTES] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
// Result / Reject payload codes, as CrossInk defines them.
constexpr uint8_t RESULT_OK = 0;
constexpr uint8_t RESULT_FAILED = 1;
constexpr uint8_t REJECT_USER = 1;
constexpr uint8_t REJECT_STORAGE = 2;
// Offer payload: u64 size, u16 chunk, u8 senderLen, u8 nameLen, sender, name.
constexpr size_t OFFER_HEADER_BYTES = 12;
// Received files land in the card root, which is where CrossInk defaults too.
constexpr const char* RECEIVE_FOLDER = "/";

std::atomic<bool> nearbyRadioActive{false};

bool sameMac(const std::array<uint8_t, nearby::MAC_BYTES>& lhs, const uint8_t* rhs) {
  return rhs && memcmp(lhs.data(), rhs, nearby::MAC_BYTES) == 0;
}
}  // namespace

NearbyBookTransferActivity::NearbyBookTransferActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                       const Mode mode, std::string sourcePath)
    : UiListActivity("NearbyBookTransfer", renderer, mappedInput), mode_(mode), sourcePath_(std::move(sourcePath)) {}

NearbyBookTransferActivity::~NearbyBookTransferActivity() { stopRadio(); }

bool NearbyBookTransferActivity::isRadioActive() { return nearbyRadioActive.load(std::memory_order_acquire); }

// --- helpers -----------------------------------------------------------------

bool NearbyBookTransferActivity::supportedFile(const std::string& path) {
  return FsHelpers::hasEpubExtension(path) || FsHelpers::hasTxtExtension(path) || FsHelpers::hasXtcExtension(path) ||
         FsHelpers::hasPngExtension(path) || FsHelpers::hasBmpExtension(path);
}

bool NearbyBookTransferActivity::safeFileName(const std::string& name) {
  if (name.empty() || name.size() > MAX_FILE_NAME_LENGTH || name.front() == '.' || name.back() == ' ' ||
      name.back() == '.') {
    return false;
  }
  return name.find_first_of("/\\:*?\"<>|") == std::string::npos && supportedFile(name);
}

std::string NearbyBookTransferActivity::fileNameFromPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string NearbyBookTransferActivity::joinPath(const std::string& folder, const std::string& name) {
  if (folder.empty() || folder == "/") return "/" + name;
  return folder.back() == '/' ? folder + name : folder + "/" + name;
}

std::string NearbyBookTransferActivity::keepBothPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  const size_t dot = path.find_last_of('.');
  const bool hasExtension = dot != std::string::npos && (slash == std::string::npos || dot > slash);
  const std::string base = hasExtension ? path.substr(0, dot) : path;
  const std::string extension = hasExtension ? path.substr(dot) : "";
  for (uint16_t suffix = 2; suffix < 1000; ++suffix) {
    char number[12];
    snprintf(number, sizeof(number), " (%u)", static_cast<unsigned>(suffix));
    const std::string candidate = base + number + extension;
    if (!Storage.exists(candidate.c_str())) return candidate;
  }
  return {};
}

// The player name is this fork's device identity (three words, <= 20 chars),
// which also fits CrossInk's 20-byte device-name field on the wire.
const char* NearbyBookTransferActivity::deviceName() { return player::name(); }

// --- lifecycle ---------------------------------------------------------------

void NearbyBookTransferActivity::onEnter() {
  UiListActivity::onEnter();

  if (mode_ == Mode::Receive) {
    startListening();
    return;
  }

  if (!supportedFile(sourcePath_)) {
    setError(tr(STR_NEARBY_TRANSFER_UNSUPPORTED));
    return;
  }
  HalFile probe;
  if (!Storage.openFileForRead(LOG_TAG, sourcePath_, probe)) {
    setError(tr(STR_NEARBY_TRANSFER_SOURCE_FAILED));
    return;
  }
  offeredFileSize_ = probe.fileSize64();
  probe.close();
  std::string name = fileNameFromPath(sourcePath_);
  if (!safeFileName(name)) {
    setError(tr(STR_NEARBY_TRANSFER_SOURCE_FAILED));
    return;
  }
  {
    RenderLock lock(*this);
    offeredFileName_ = std::move(name);
  }
  startDiscovery();
}

void NearbyBookTransferActivity::onExit() {
  sourceFile_.close();
  receiveFile_.close();
  stopRadio();
  UiListActivity::onExit();
}

bool NearbyBookTransferActivity::skipLoopDelay() {
  const State s = state_.load();
  return s == State::Discovering || s == State::WaitingForApproval || s == State::Sending || s == State::Receiving ||
         s == State::Listening;
}

// --- radio -------------------------------------------------------------------

bool NearbyBookTransferActivity::startRadio() {
  if (transport_.started()) return true;
  if (linkplay::Radio::isActive()) {
    setError(tr(STR_NEARBY_TRANSFER_RADIO_BUSY));
    return false;
  }

  // Rebuildable SD-font glyph arenas give contiguous internal DRAM back before
  // Wi-Fi initializes, which matters when launched over an open reader/library.
  if (auto* fcm = renderer.getFontCacheManager()) {
    fcm->releaseSdFontCaches();
  }

  devmode::pause();
  radioHeld_ = true;
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
  // Same reasoning as LinkRadio::begin(): disarm auto-reconnect and wait for a
  // real disassociation, or the channel pin below is reverted by a Developer
  // Mode association still in flight.
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect(false, false, 500);
#endif
  if (!transport_.begin(ESPNOW_CHANNEL)) {
    setError(tr(STR_NEARBY_TRANSFER_RADIO_FAILED));
    return false;
  }
  nearbyRadioActive.store(true, std::memory_order_release);
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
  LOG_INF(LOG_TAG, "radio ready: free=%u maxAlloc=%u", ESP.getFreeHeap(), ESP.getMaxAllocHeap());
#endif
  return true;
}

void NearbyBookTransferActivity::stopRadio() {
  nearbyRadioActive.store(false, std::memory_order_release);
  transport_.end();  // also puts Wi-Fi back to WIFI_OFF
  if (!radioHeld_) return;
  radioHeld_ = false;
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
  WiFi.setAutoReconnect(true);
#endif
  devmode::resume();
}

void NearbyBookTransferActivity::startListening() {
  if (!startRadio()) return;
  session_.reset();
  syncProgressSnapshot();
  peerMac_ = {};
  {
    RenderLock lock(*this);
    offeredFileName_.clear();
    senderName_.clear();
    finalPath_.clear();
    tempPath_.clear();
  }
  retryCount_ = 0;
  setState(State::Listening);
}

void NearbyBookTransferActivity::startDiscovery() {
  if (!startRadio()) return;
#if defined(ARDUINO_ARCH_ESP32) && !defined(SIMULATOR)
  sessionId_ = esp_random();
#else
  sessionId_ = static_cast<uint32_t>(millis()) ^ 0x43494654u;
#endif
  if (sessionId_ == 0) sessionId_ = 1;
  {
    RenderLock lock(*this);
    peerCount_.store(0, std::memory_order_release);
  }
  lastActionMs_ = 0;
  setState(State::Discovering);
  sendDiscovery();
}

// --- protocol: sending packets -----------------------------------------------

bool NearbyBookTransferActivity::sendPacket(const nearby::PacketType type, const uint8_t* destination,
                                            const uint32_t sequence, const void* payload,
                                            const uint16_t payloadLength) {
  size_t length = 0;
  if (!nearby::encodePacket(packetBuffer_.data(), packetBuffer_.size(), type, sessionId_, sequence, payload,
                            payloadLength, length)) {
    return false;
  }
  return transport_.send(destination, packetBuffer_.data(), length);
}

bool NearbyBookTransferActivity::sendDiscovery() {
  lastActionMs_ = millis();
  return sendPacket(nearby::PacketType::Discover, BROADCAST_MAC);
}

bool NearbyBookTransferActivity::sendAdvertisement(const uint8_t* destination) {
  const char* name = deviceName();
  const auto length = static_cast<uint16_t>(std::min(strlen(name), MAX_DEVICE_NAME_LENGTH));
  return sendPacket(nearby::PacketType::Advertise, destination, 0, name, length);
}

bool NearbyBookTransferActivity::sendOffer() {
  const char* name = deviceName();
  const size_t senderLength = std::min(strlen(name), MAX_DEVICE_NAME_LENGTH);
  if (offeredFileName_.size() > MAX_FILE_NAME_LENGTH || senderLength == 0) return false;
  std::array<uint8_t, OFFER_HEADER_BYTES + MAX_DEVICE_NAME_LENGTH + MAX_FILE_NAME_LENGTH> payload{};
  nearby::writeU64(payload.data(), offeredFileSize_);
  nearby::writeU16(payload.data() + 8, nearby::V2_CHUNK_BYTES);
  payload[10] = static_cast<uint8_t>(senderLength);
  payload[11] = static_cast<uint8_t>(offeredFileName_.size());
  memcpy(payload.data() + OFFER_HEADER_BYTES, name, senderLength);
  memcpy(payload.data() + OFFER_HEADER_BYTES + senderLength, offeredFileName_.data(), offeredFileName_.size());
  lastActionMs_ = millis();
  return sendPacket(nearby::PacketType::Offer, peerMac_.data(), 0, payload.data(),
                    static_cast<uint16_t>(OFFER_HEADER_BYTES + senderLength + offeredFileName_.size()));
}

bool NearbyBookTransferActivity::sendAccept() {
  uint8_t payload[2];
  nearby::writeU16(payload, negotiatedChunkBytes_);
  lastActionMs_ = millis();
  return sendPacket(nearby::PacketType::Accept, peerMac_.data(), 0, payload, sizeof(payload));
}

bool NearbyBookTransferActivity::sendNextChunk() {
  pendingChunkLength_ = 0;
  const int bytesRead = sourceFile_.read(chunkBuffer_.data(), negotiatedChunkBytes_);
  if (bytesRead <= 0) {
    setError(tr(STR_NEARBY_TRANSFER_SOURCE_FAILED));
    return false;
  }
  pendingChunkLength_ = static_cast<size_t>(bytesRead);
  session_.includeBytes(chunkBuffer_.data(), pendingChunkLength_);
  retryCount_ = 0;
  return resendPending();
}

bool NearbyBookTransferActivity::resendPending() {
  lastActionMs_ = millis();
  return sendPacket(nearby::PacketType::Data, peerMac_.data(), session_.nextSequence(), chunkBuffer_.data(),
                    static_cast<uint16_t>(pendingChunkLength_));
}

bool NearbyBookTransferActivity::sendAck() {
  uint8_t payload[4];
  nearby::writeU32(payload, session_.nextSequence());
  lastActionMs_ = millis();
  return sendPacket(nearby::PacketType::Ack, peerMac_.data(), 0, payload, sizeof(payload));
}

bool NearbyBookTransferActivity::sendComplete() {
  uint8_t payload[12];
  nearby::writeU64(payload, session_.totalBytes());
  nearby::writeU32(payload + 8, session_.crc32());
  lastActionMs_ = millis();
  retryCount_ = 0;
  return sendPacket(nearby::PacketType::Complete, peerMac_.data(), 0, payload, sizeof(payload));
}

// --- protocol: receiving packets ---------------------------------------------

void NearbyBookTransferActivity::processPackets() {
  if (!transport_.started()) return;
  while (transport_.poll(eventBuffer_)) {
    nearby::PacketView packet;
    if (nearby::decodePacket(eventBuffer_.data.data(), eventBuffer_.length, packet)) handlePacket(eventBuffer_, packet);
  }
}

void NearbyBookTransferActivity::handlePacket(const nearby::EspNowTransport::Event& event,
                                              const nearby::PacketView& packet) {
  if (packet.type == nearby::PacketType::Discover && state_ == State::Listening) {
    sessionId_ = packet.sessionId;
    sendAdvertisement(event.sourceMac.data());
    return;
  }
  if (mode_ == Mode::Send) {
    if (packet.sessionId == sessionId_) handleSenderPacket(event, packet);
  } else {
    handleReceiverPacket(event, packet);
  }
}

void NearbyBookTransferActivity::handleSenderPacket(const nearby::EspNowTransport::Event& event,
                                                    const nearby::PacketView& packet) {
  const State s = state_.load();
  if (packet.type == nearby::PacketType::Advertise && (s == State::Discovering || s == State::DeviceList) &&
      packet.payloadLength >= 2 && packet.payloadLength <= MAX_DEVICE_NAME_LENGTH) {
    const uint8_t count = peerCount_.load(std::memory_order_relaxed);
    for (uint8_t i = 0; i < count; ++i) {
      if (sameMac(peers_[i].mac, event.sourceMac.data())) return;
    }
    if (count >= MAX_PEERS) return;
    {
      RenderLock lock(*this);
      closeRouting();
      Peer& peer = peers_[count];
      peer.mac = event.sourceMac;
      memcpy(peer.name.data(), packet.payload, packet.payloadLength);
      peer.name[packet.payloadLength] = '\0';
      peerCount_.store(static_cast<uint8_t>(count + 1), std::memory_order_release);
    }
    if (s == State::Discovering) {
      setState(State::DeviceList);
    } else {
      requestUpdate();
    }
    return;
  }

  if (!sameMac(peerMac_, event.sourceMac.data())) return;

  if (packet.type == nearby::PacketType::Accept && s == State::WaitingForApproval && packet.payloadLength == 2) {
    negotiatedChunkBytes_ =
        std::clamp<uint16_t>(nearby::readU16(packet.payload), nearby::COMPAT_CHUNK_BYTES, nearby::V2_CHUNK_BYTES);
    if (!Storage.openFileForRead(LOG_TAG, sourcePath_, sourceFile_)) {
      setError(tr(STR_NEARBY_TRANSFER_SOURCE_FAILED));
      return;
    }
    session_.begin(nearby::ReliableTransferSession::Role::Sender, sessionId_, offeredFileSize_, negotiatedChunkBytes_);
    syncProgressSnapshot();
    retryCount_ = 0;
    setState(State::Sending);
    if (offeredFileSize_ == 0) {
      sourceFile_.close();
      sendComplete();
    } else {
      sendNextChunk();
    }
    return;
  }
  if (packet.type == nearby::PacketType::Reject && s == State::WaitingForApproval) {
    setError(packet.payloadLength == 1 && packet.payload[0] == REJECT_STORAGE ? tr(STR_NEARBY_TRANSFER_NO_SPACE)
                                                                              : tr(STR_NEARBY_TRANSFER_REJECTED));
    return;
  }
  if (packet.type == nearby::PacketType::Ack && s == State::Sending && packet.payloadLength == 4) {
    if (!session_.acceptAcknowledgement(nearby::readU32(packet.payload))) return;
    session_.advanceSentBytes(pendingChunkLength_);
    syncProgressSnapshot();
    pendingChunkLength_ = 0;
    retryCount_ = 0;
    maybeRefreshProgress();
    if (session_.transferredBytes() == session_.totalBytes()) {
      sourceFile_.close();
      sendComplete();
    } else {
      sendNextChunk();
    }
    return;
  }
  if (packet.type == nearby::PacketType::Result && s == State::Sending && packet.payloadLength == 1) {
    if (packet.payload[0] == RESULT_OK) {
      setState(State::Success);
    } else {
      setError(tr(STR_NEARBY_TRANSFER_VERIFY_FAILED));
    }
    return;
  }
  if (packet.type == nearby::PacketType::Cancel) setError(tr(STR_NEARBY_TRANSFER_CANCELLED));
}

void NearbyBookTransferActivity::handleReceiverPacket(const nearby::EspNowTransport::Event& event,
                                                      const nearby::PacketView& packet) {
  const State s = state_.load();
  if (packet.type == nearby::PacketType::Offer && s == State::Listening && packet.payloadLength >= 14) {
    const uint8_t senderLength = packet.payload[10];
    const uint8_t nameLength = packet.payload[11];
    if (senderLength == 0 || senderLength > MAX_DEVICE_NAME_LENGTH || nameLength == 0 ||
        nameLength > MAX_FILE_NAME_LENGTH || packet.payloadLength != OFFER_HEADER_BYTES + senderLength + nameLength) {
      return;
    }
    std::string name(reinterpret_cast<const char*>(packet.payload + OFFER_HEADER_BYTES + senderLength), nameLength);
    if (!safeFileName(name)) return;
    sessionId_ = packet.sessionId;
    peerMac_ = event.sourceMac;
    offeredFileSize_ = nearby::readU64(packet.payload);
    negotiatedChunkBytes_ =
        std::clamp<uint16_t>(nearby::readU16(packet.payload + 8), nearby::COMPAT_CHUNK_BYTES, nearby::V2_CHUNK_BYTES);
    {
      RenderLock lock(*this);
      offeredFileName_ = std::move(name);
      senderName_.assign(reinterpret_cast<const char*>(packet.payload + OFFER_HEADER_BYTES), senderLength);
      finalPath_ = joinPath(RECEIVE_FOLDER, offeredFileName_);
    }
    setState(State::OfferPrompt);
    return;
  }

  if (packet.sessionId != sessionId_ || !sameMac(peerMac_, event.sourceMac.data())) return;

  // The sender retries its Offer until it hears Accept; answer repeats so a
  // lost Accept does not stall the transfer.
  if (packet.type == nearby::PacketType::Offer && s == State::Receiving) {
    sendAccept();
    return;
  }
  if (packet.type == nearby::PacketType::Data && s == State::Receiving) {
    if (packet.sequence != session_.nextSequence() || packet.payloadLength == 0 ||
        packet.payloadLength > negotiatedChunkBytes_) {
      sendAck();  // duplicate or out-of-order: restate where we are
      return;
    }
    const size_t written = receiveFile_.write(packet.payload, packet.payloadLength);
    if (written != packet.payloadLength ||
        !session_.acceptReceivedChunk(packet.sequence, static_cast<size_t>(packet.payloadLength))) {
      const uint8_t failed = RESULT_FAILED;
      sendPacket(nearby::PacketType::Result, peerMac_.data(), 0, &failed, 1);
      setError(tr(STR_NEARBY_TRANSFER_WRITE_FAILED));
      return;
    }
    session_.includeBytes(packet.payload, packet.payloadLength);
    syncProgressSnapshot();
    sendAck();
    maybeRefreshProgress();
    return;
  }
  if (packet.type == nearby::PacketType::Complete && (s == State::Receiving || s == State::Success) &&
      packet.payloadLength == 12) {
    const uint64_t expectedBytes = nearby::readU64(packet.payload);
    const uint32_t expectedCrc = nearby::readU32(packet.payload + 8);
    // A repeated Complete after success means our Result was lost; re-send it.
    const bool success = s == State::Success || finishReceivedFile(expectedBytes, expectedCrc);
    const uint8_t result = success ? RESULT_OK : RESULT_FAILED;
    sendPacket(nearby::PacketType::Result, peerMac_.data(), 0, &result, 1);
    if (success && s != State::Success) setState(State::Success);
    return;
  }
  if (packet.type == nearby::PacketType::Cancel) setError(tr(STR_NEARBY_TRANSFER_CANCELLED));
}

// --- receiver decisions -------------------------------------------------------

void NearbyBookTransferActivity::acceptOffer(const bool keepBoth) {
  setState(State::Validating);
  requestUpdateAndWait();

  if (keepBoth) {
    std::string candidate = keepBothPath(finalPath_);
    if (candidate.empty()) {
      setError(tr(STR_NEARBY_TRANSFER_NAME_FAILED));
      return;
    }
    RenderLock lock(*this);
    finalPath_ = std::move(candidate);
  }

  uint64_t freeBytes = 0;
  if (Storage.freeBytes(freeBytes) && offeredFileSize_ > freeBytes) {
    const uint8_t reason = REJECT_STORAGE;
    sendPacket(nearby::PacketType::Reject, peerMac_.data(), 0, &reason, 1);
    setError(tr(STR_NEARBY_TRANSFER_NO_SPACE));
    return;
  }

  // Same hidden staging names CrossInk uses, so a card moved between the two
  // firmwares never has two kinds of leftovers.
  const std::string finalName = fileNameFromPath(finalPath_);
  {
    RenderLock lock(*this);
    tempPath_ = joinPath(RECEIVE_FOLDER, "." + finalName + ".crossink-part");
    backupPath_ = joinPath(RECEIVE_FOLDER, "." + finalName + ".crossink-backup");
  }
  if (Storage.exists(tempPath_.c_str())) Storage.remove(tempPath_.c_str());
  if (!Storage.openFileForWrite(LOG_TAG, tempPath_, receiveFile_)) {
    setError(tr(STR_NEARBY_TRANSFER_WRITE_FAILED));
    return;
  }
  session_.begin(nearby::ReliableTransferSession::Role::Receiver, sessionId_, offeredFileSize_, negotiatedChunkBytes_);
  syncProgressSnapshot();
  retryCount_ = 0;
  setState(State::Receiving);
  // Paint the progress screen BEFORE accepting: a panel refresh mid-stream
  // stalls the loop long enough to cost retransmits on the first chunks.
  requestUpdateAndWait();
  lastUiMs_ = millis();
  if (!sendAccept()) setError(tr(STR_NEARBY_TRANSFER_RADIO_FAILED));
}

void NearbyBookTransferActivity::rejectOffer() {
  const uint8_t reason = REJECT_USER;
  sendPacket(nearby::PacketType::Reject, peerMac_.data(), 0, &reason, 1);
  startListening();
}

bool NearbyBookTransferActivity::finishReceivedFile(const uint64_t expectedBytes, const uint32_t expectedCrc) {
  if (expectedBytes != session_.totalBytes() || expectedBytes != session_.transferredBytes() ||
      expectedCrc != session_.crc32()) {
    receiveFile_.close();
    Storage.remove(tempPath_.c_str());
    setError(tr(STR_NEARBY_TRANSFER_VERIFY_FAILED));
    return false;
  }
  receiveFile_.flush();
  if (!receiveFile_.close()) {
    Storage.remove(tempPath_.c_str());
    setError(tr(STR_NEARBY_TRANSFER_WRITE_FAILED));
    return false;
  }
  const bool replacing = Storage.exists(finalPath_.c_str());
  if (replacing) {
    if (Storage.exists(backupPath_.c_str())) Storage.remove(backupPath_.c_str());
    if (!Storage.rename(finalPath_.c_str(), backupPath_.c_str())) {
      Storage.remove(tempPath_.c_str());
      setError(tr(STR_NEARBY_TRANSFER_REPLACE_FAILED));
      return false;
    }
  }
  if (!Storage.rename(tempPath_.c_str(), finalPath_.c_str())) {
    if (replacing) Storage.rename(backupPath_.c_str(), finalPath_.c_str());
    Storage.remove(tempPath_.c_str());
    setError(tr(STR_NEARBY_TRANSFER_REPLACE_FAILED));
    return false;
  }
  if (replacing) Storage.remove(backupPath_.c_str());
  library::markLibraryIndexDirty();
  LOG_INF(LOG_TAG, "received %s (%llu bytes)", finalPath_.c_str(), static_cast<unsigned long long>(expectedBytes));
  return true;
}

void NearbyBookTransferActivity::cancelTransfer() {
  if (transport_.started() && peerMac_ != std::array<uint8_t, nearby::MAC_BYTES>{}) {
    sendPacket(nearby::PacketType::Cancel, peerMac_.data());
  }
  sourceFile_.close();
  receiveFile_.close();
  if (!tempPath_.empty() && Storage.exists(tempPath_.c_str())) Storage.remove(tempPath_.c_str());
}

// --- timers --------------------------------------------------------------------

void NearbyBookTransferActivity::updateTimers() {
  const uint32_t now = millis();
  const State s = state_.load();
  if (s == State::Discovering || s == State::DeviceList) {
    // Keep asking while the list is up so late receivers still appear.
    if (now - lastActionMs_ >= DISCOVERY_INTERVAL_MS) sendDiscovery();
    return;
  }
  if (s == State::WaitingForApproval && now - lastActionMs_ >= RETRY_INTERVAL_MS * 2) {
    if (++retryCount_ > MAX_APPROVAL_RETRIES) {
      setError(tr(STR_NEARBY_TRANSFER_TIMEOUT));
    } else {
      sendOffer();
    }
    return;
  }
  if (s == State::Receiving && now - lastActionMs_ >= RECEIVE_TIMEOUT_MS) {
    setError(tr(STR_NEARBY_TRANSFER_TIMEOUT));
    return;
  }
  if (s != State::Sending || now - lastActionMs_ < RETRY_INTERVAL_MS) return;
  if (++retryCount_ > MAX_RETRIES) {
    setError(tr(STR_NEARBY_TRANSFER_TIMEOUT));
    return;
  }
  if (pendingChunkLength_ > 0) {
    resendPending();
  } else {
    sendComplete();
  }
}

void NearbyBookTransferActivity::syncProgressSnapshot() {
  const uint64_t done = session_.transferredBytes();
  const uint64_t total = session_.totalBytes();
  transferredKb_.store(static_cast<uint32_t>(done / 1024), std::memory_order_relaxed);
  totalKb_.store(static_cast<uint32_t>(total / 1024), std::memory_order_relaxed);
  const uint32_t permille = total > 0 ? static_cast<uint32_t>(std::min<uint64_t>(1000, (done * 1000) / total)) : 1000;
  progressPermille_.store(permille, std::memory_order_relaxed);
}

void NearbyBookTransferActivity::maybeRefreshProgress() {
  if (millis() - lastUiMs_ < UI_REFRESH_MS) return;
  lastUiMs_ = millis();
  requestUpdate();
}

// --- flow ----------------------------------------------------------------------

void NearbyBookTransferActivity::setState(const State state) {
  app.clearTapFlash();
  {
    RenderLock lock(*this);
    // Invalidate the previous state's published hit table before changing the
    // meaning of row 0..N; renderUi() reopens routing once the new rows paint.
    closeRouting();
    state_.store(state, std::memory_order_release);
    nav.selected = 0;
    nav.top = 0;
    nav.followOnBuild = true;
  }
  lastUiMs_ = millis();
  requestUpdate();
}

void NearbyBookTransferActivity::setError(const char* message) {
  const char* resolved = message ? message : tr(STR_NEARBY_TRANSFER_FAILED);
  errorMessage_.store(resolved, std::memory_order_release);
  LOG_ERR(LOG_TAG, "%s", resolved);
  cancelTransfer();
  setState(State::Error);
}

void NearbyBookTransferActivity::exitTransfer() {
  const State s = state_.load();
  if (s == State::Sending || s == State::Receiving || s == State::WaitingForApproval || s == State::OfferPrompt ||
      s == State::CollisionPrompt || s == State::Validating) {
    cancelTransfer();
  }
  stopRadio();
  finish();
}

void NearbyBookTransferActivity::openReceivedFile() {
  if (finalPath_.empty()) return;
  stopRadio();
  onSelectBook(finalPath_);
}

void NearbyBookTransferActivity::loop() {
  processPackets();
  updateTimers();
  UiListActivity::loop();
}

void NearbyBookTransferActivity::onBackButton() {
  const State s = state_.load();
  if (s == State::OfferPrompt || s == State::CollisionPrompt) {
    rejectOffer();
    return;
  }
  exitTransfer();
}

void NearbyBookTransferActivity::selectPeer(const int index) {
  const uint8_t count = peerCount_.load(std::memory_order_acquire);
  if (index < 0 || index >= count) return;
  chosenPeer_.store(static_cast<uint8_t>(index), std::memory_order_release);
  peerMac_ = peers_[index].mac;
  retryCount_ = 0;
  setState(State::WaitingForApproval);
  sendOffer();
}

// --- rows ----------------------------------------------------------------------

int NearbyBookTransferActivity::listCount() const {
  switch (state_.load(std::memory_order_acquire)) {
    case State::DeviceList:
      return peerCount_.load(std::memory_order_acquire) + 1;  // peers, then Cancel
    case State::OfferPrompt:
      return 2;  // Accept, Decline
    case State::CollisionPrompt:
      return 3;  // Replace, Keep both, Cancel
    case State::Success:
      return mode_ == Mode::Receive ? 2 : 1;  // [Read now,] Done
    case State::Validating:
      return 0;
    default:
      return 1;  // Cancel / OK
  }
}

void NearbyBookTransferActivity::activateIndex(const int index) {
  app.clearTapFlash();
  switch (state_.load(std::memory_order_acquire)) {
    case State::DeviceList:
      if (index < peerCount_.load(std::memory_order_acquire)) {
        selectPeer(index);
      } else {
        exitTransfer();
      }
      return;
    case State::OfferPrompt:
      if (index != 0) {
        rejectOffer();
      } else if (Storage.exists(finalPath_.c_str())) {
        setState(State::CollisionPrompt);
      } else {
        acceptOffer(false);
      }
      return;
    case State::CollisionPrompt:
      if (index == 0) {
        acceptOffer(false);
      } else if (index == 1) {
        acceptOffer(true);
      } else {
        rejectOffer();
      }
      return;
    case State::Success:
      if (mode_ == Mode::Receive && index == 0) {
        openReceivedFile();
      } else {
        exitTransfer();
      }
      return;
    case State::Validating:
      return;
    default:
      exitTransfer();
      return;
  }
}

void NearbyBookTransferActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int count = std::min<int>(listCount(), MAX_ROWS);
  const State s = state_.load(std::memory_order_acquire);
  const uint8_t peers = peerCount_.load(std::memory_order_acquire);
  for (int i = 0; i < count; ++i) {
    fui::ListItem item;
    item.actionValue = static_cast<int16_t>(i);
    switch (s) {
      case State::DeviceList:
        item.label = i < peers ? peers_[i].name.data() : tr(STR_CANCEL);
        break;
      case State::OfferPrompt:
        item.label = i == 0 ? tr(STR_ACCEPT) : tr(STR_DECLINE);
        break;
      case State::CollisionPrompt:
        item.label = i == 0 ? tr(STR_REPLACE) : (i == 1 ? tr(STR_KEEP_BOTH) : tr(STR_CANCEL));
        break;
      case State::Success:
        if (mode_ == Mode::Receive && i == 0) {
          const bool image = FsHelpers::hasPngExtension(finalPath_) || FsHelpers::hasBmpExtension(finalPath_);
          item.label = image ? tr(STR_OPEN) : tr(STR_READ_NOW);
        } else {
          item.label = tr(STR_DONE);
        }
        break;
      case State::Error:
        item.label = tr(STR_DONE);
        break;
      default:
        item.label = tr(STR_CANCEL);
        break;
    }
    rowItems_[i] = item;
  }

  // The upper band holds the status text drawn by drawChrome(); rows sit below.
  const int textBand = renderer.getScreenHeight() * 2 / 5;
  screen.setContentMarginFromScreen(
      fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight + textBand), 0,
                  static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  fui::ListProps props;
  props.items = rowItems_;
  props.count = static_cast<uint16_t>(count);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}

const char* NearbyBookTransferActivity::headerTitle() const {
  return mode_ == Mode::Send ? tr(STR_SEND_NEARBY_BOOK) : tr(STR_NEARBY_BOOK_TRANSFER);
}

void NearbyBookTransferActivity::drawChrome() {
  UiListActivity::drawChrome();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int width = renderer.getScreenWidth();
  const int textWidth = width - metrics.contentSidePadding * 2;
  const int bandTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing * 2;
  const int titleLine = renderer.getLineHeight(UI_12_FONT_ID);
  const int smallLine = renderer.getLineHeight(SMALL_FONT_ID);
  int y = bandTop;

  auto title = [&](const char* text) {
    for (const auto& line : renderer.wrappedText(UI_12_FONT_ID, text, textWidth, 2, EpdFontFamily::BOLD)) {
      renderer.drawCenteredText(UI_12_FONT_ID, y, line.c_str(), true, EpdFontFamily::BOLD);
      y += titleLine;
    }
    y += metrics.verticalSpacing;
  };
  auto detail = [&](const char* text, const int maxLines = 2) {
    if (!text || !*text) return;
    for (const auto& line : renderer.wrappedText(SMALL_FONT_ID, text, textWidth, maxLines)) {
      renderer.drawCenteredText(SMALL_FONT_ID, y, line.c_str());
      y += smallLine;
    }
  };
  auto progress = [&]() {
    y += metrics.verticalSpacing;
    const int barY = y;
    GUI.drawProgressBar(renderer, Rect{metrics.contentSidePadding, barY, textWidth, metrics.progressBarHeight},
                        progressPermille_.load(std::memory_order_relaxed), 1000);
    // BaseTheme::drawProgressBar renders the centered percentage text at (barY + progressBarHeight + 15).
    // Advance past the progress bar, percentage text, and spacing before drawing the transferred bytes.
    const int percentHeight = renderer.getLineHeight(UI_10_FONT_ID);
    y = barY + metrics.progressBarHeight + 15 + percentHeight + metrics.verticalSpacing;
    char text[48];
    snprintf(text, sizeof(text), "%lu / %lu KB",
             static_cast<unsigned long>(transferredKb_.load(std::memory_order_relaxed)),
             static_cast<unsigned long>(totalKb_.load(std::memory_order_relaxed)));
    renderer.drawCenteredText(SMALL_FONT_ID, y, text);
  };
  char sizeText[32];
  snprintf(sizeText, sizeof(sizeText), "%llu KB", static_cast<unsigned long long>((offeredFileSize_ + 1023) / 1024));

  switch (state_.load(std::memory_order_acquire)) {
    case State::Listening:
      title(tr(STR_NEARBY_TRANSFER_LISTENING));
      detail(deviceName());
      break;
    case State::Discovering:
      title(tr(STR_NEARBY_TRANSFER_DISCOVERING));
      detail(offeredFileName_.c_str());
      break;
    case State::DeviceList:
      title(tr(STR_NEARBY_TRANSFER_CHOOSE_DEVICE));
      detail(offeredFileName_.c_str());
      break;
    case State::WaitingForApproval:
      title(tr(STR_NEARBY_TRANSFER_WAITING_APPROVAL));
      detail(peers_[chosenPeer_.load(std::memory_order_acquire)].name.data(), 1);
      break;
    case State::OfferPrompt:
      title(tr(STR_NEARBY_TRANSFER_INCOMING));
      detail(senderName_.c_str(), 1);
      detail(offeredFileName_.c_str());
      detail(sizeText, 1);
      break;
    case State::CollisionPrompt:
      title(tr(STR_NEARBY_TRANSFER_EXISTS));
      detail(offeredFileName_.c_str());
      break;
    case State::Validating:
      title(tr(STR_NEARBY_TRANSFER_VALIDATING));
      break;
    case State::Sending:
      title(tr(STR_NEARBY_TRANSFER_SENDING));
      detail(offeredFileName_.c_str(), 1);
      progress();
      break;
    case State::Receiving:
      title(tr(STR_NEARBY_TRANSFER_RECEIVING));
      detail(offeredFileName_.c_str(), 1);
      progress();
      break;
    case State::Success:
      title(mode_ == Mode::Receive ? tr(STR_NEARBY_TRANSFER_RECEIVED) : tr(STR_NEARBY_TRANSFER_SENT));
      detail(mode_ == Mode::Receive ? finalPath_.c_str() : offeredFileName_.c_str(), 3);
      break;
    case State::Error:
      title(tr(STR_NEARBY_TRANSFER_FAILED));
      detail(errorMessage_.load(std::memory_order_acquire), 3);
      break;
  }
}

void NearbyBookTransferActivity::drawFooter() {
  const bool navigable = listCount() > 1;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), listCount() > 0 ? tr(STR_SELECT) : "",
                                            navigable ? tr(STR_DIR_UP) : "", navigable ? tr(STR_DIR_DOWN) : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
}
