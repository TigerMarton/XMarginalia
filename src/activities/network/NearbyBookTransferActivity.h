#pragma once

#include <HalStorage.h>
#include <NearbyTransfer.h>

#include <array>
#include <atomic>
#include <string>

#include "activities/UiListActivity.h"

// Device-to-device file transfer over ESP-NOW, wire-compatible with CrossInk's
// NearbyBookTransferActivity (same CIFT packet framing from freeink-sdk and the
// same Offer/Accept/Data/Ack/Complete/Result payload layouts), so a Picco, an
// X4 Pro and a CrossInk X3/X4 can all exchange books with each other.
//
// Every decision is a list row rather than a button-hint action, so the flow
// works the same on touch-only boards (Picco) and button boards: the header
// back target and Back both cancel, and rows are tappable or Confirm-able.
//
// Radio ownership: ESP-NOW has a single global receive callback, shared with
// the multiplayer link (apps_local/link/LinkRadio). Both sides check the
// other's active flag before claiming ESP-NOW rather than silently stealing
// the callback, and Developer Mode is paused while the transport is up.
class NearbyBookTransferActivity final : public UiListActivity {
 public:
  enum class Mode : uint8_t { Send, Receive };

  NearbyBookTransferActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode,
                             std::string sourcePath = {});
  ~NearbyBookTransferActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override;

  // Extensions this transfer will send or accept; matches CrossInk's list.
  static bool supportedFile(const std::string& path);

  // True while an instance holds ESP-NOW; queried by LinkRadio::begin().
  static bool isRadioActive();

 private:
  enum class State : uint8_t {
    Listening,
    Discovering,
    DeviceList,
    WaitingForApproval,
    OfferPrompt,
    CollisionPrompt,
    Validating,
    Sending,
    Receiving,
    Success,
    Error,
  };

  struct Peer {
    std::array<uint8_t, freeink::nearby::MAC_BYTES> mac{};
    std::array<char, 21> name{};
  };

  static constexpr size_t MAX_PEERS = 4;
  static constexpr size_t MAX_ROWS = MAX_PEERS + 1;
  static constexpr size_t MAX_DEVICE_NAME_LENGTH = 20;
  static constexpr size_t MAX_FILE_NAME_LENGTH = 180;
  static constexpr uint8_t ESPNOW_CHANNEL = 1;
  static constexpr uint32_t DISCOVERY_INTERVAL_MS = 900;
  static constexpr uint32_t RETRY_INTERVAL_MS = 450;
  static constexpr uint8_t MAX_RETRIES = 12;
  static constexpr uint8_t MAX_APPROVAL_RETRIES = 60;
  static constexpr uint32_t RECEIVE_TIMEOUT_MS = 15000;
  static constexpr uint32_t UI_REFRESH_MS = 1800;

  Mode mode_;
  std::atomic<State> state_{State::Listening};
  std::string sourcePath_;
  bool radioHeld_ = false;

  freeink::nearby::EspNowTransport transport_;
  freeink::nearby::EspNowTransport::Event eventBuffer_;
  freeink::nearby::ReliableTransferSession session_;
  std::array<Peer, MAX_PEERS> peers_{};
  std::atomic<uint8_t> peerCount_{0};
  std::atomic<uint8_t> chosenPeer_{0};
  std::array<uint8_t, freeink::nearby::MAC_BYTES> peerMac_{};

  HalFile sourceFile_;
  HalFile receiveFile_;
  std::array<uint8_t, freeink::nearby::V2_CHUNK_BYTES> chunkBuffer_{};
  std::array<uint8_t, freeink::nearby::MAX_PACKET_BYTES> packetBuffer_{};
  size_t pendingChunkLength_ = 0;
  uint64_t offeredFileSize_ = 0;
  uint16_t negotiatedChunkBytes_ = freeink::nearby::COMPAT_CHUNK_BYTES;
  uint32_t sessionId_ = 0;
  uint32_t lastActionMs_ = 0;
  uint32_t lastUiMs_ = 0;
  uint8_t retryCount_ = 0;

  // 32-bit snapshots for lock-free render-task progress display (64-bit reads
  // tear on 32-bit cores).
  std::atomic<uint32_t> progressPermille_{0};
  std::atomic<uint32_t> transferredKb_{0};
  std::atomic<uint32_t> totalKb_{0};
  std::atomic<const char*> errorMessage_{nullptr};

  // Strings the render task reads; written only under RenderLock.
  std::string offeredFileName_;
  std::string senderName_;
  std::string finalPath_;
  std::string tempPath_;
  std::string backupPath_;

  freeink::ui::ListItem rowItems_[MAX_ROWS]{};

  // UiListActivity contract
  int listCount() const override;
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;
  void drawChrome() override;
  void drawFooter() override;

  // Radio
  bool startRadio();
  void stopRadio();
  void startListening();
  void startDiscovery();

  // Protocol
  void processPackets();
  void handlePacket(const freeink::nearby::EspNowTransport::Event& event, const freeink::nearby::PacketView& packet);
  void handleSenderPacket(const freeink::nearby::EspNowTransport::Event& event,
                          const freeink::nearby::PacketView& packet);
  void handleReceiverPacket(const freeink::nearby::EspNowTransport::Event& event,
                            const freeink::nearby::PacketView& packet);
  bool sendPacket(freeink::nearby::PacketType type, const uint8_t* destination, uint32_t sequence = 0,
                  const void* payload = nullptr, uint16_t payloadLength = 0);
  bool sendDiscovery();
  bool sendAdvertisement(const uint8_t* destination);
  bool sendOffer();
  bool sendAccept();
  bool sendNextChunk();
  bool resendPending();
  bool sendAck();
  bool sendComplete();
  void selectPeer(int index);
  void acceptOffer(bool keepBoth);
  void rejectOffer();
  bool finishReceivedFile(uint64_t expectedBytes, uint32_t expectedCrc);
  void cancelTransfer();
  void updateTimers();
  void syncProgressSnapshot();

  // Flow
  void setState(State state);
  void setError(const char* message);
  void exitTransfer();
  void openReceivedFile();
  void maybeRefreshProgress();

  static const char* deviceName();
  static bool safeFileName(const std::string& name);
  static std::string fileNameFromPath(const std::string& path);
  static std::string joinPath(const std::string& folder, const std::string& name);
  static std::string keepBothPath(const std::string& path);
};
