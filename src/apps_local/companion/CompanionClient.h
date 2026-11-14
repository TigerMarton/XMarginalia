#pragma once

#include <string>

#include "CompanionCore.h"

namespace companion {

class CompanionClient {
 public:
  static constexpr const char* kConfigPath = "/.crosspoint/companion.json";
  static constexpr const char* kRootsOverridePath = "/.crosspoint/companion-roots.pem";

  // Check whether /.crosspoint/companion.json exists and is enabled, or whether
  // the simulator environment variable CROSSPLAY_COMPANION_URL is set.
  static bool isConfigured();

  // Load config from /.crosspoint/companion.json (or simulator env).
  static bool loadConfig(CompanionConfig& out);

  // Execute a rapid on-demand Wi-Fi wake -> POST /v1/companion/query -> immediate
  // radio teardown cycle. If Wi-Fi was already connected (e.g. DevMode), leaves it
  // up; if this call brought Wi-Fi up from saved credentials, powers the radio off
  // before returning.
  static CompanionResponse query(const CompanionQuery& req);
};

}  // namespace companion
