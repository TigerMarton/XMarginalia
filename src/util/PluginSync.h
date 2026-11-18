#pragma once

#include <string>
#include <string_view>

#include "PluginCfi.h"

// Declarative two-way progress synchronization for SD-card plugins ("sync" block
// in device.json). Lets any service with a REST progress endpoint (such as
// Audiobookshelf) participate in the reader's Sync Progress flow without
// service-specific firmware code or a separate KOSync server.
namespace pluginsync {

struct RemoteProgress {
  float percentage = 0.0f;  // 0.0 .. 1.0
  std::string location;     // Optional opaque or EPUB CFI location string
  int spineHint = -1;       // Parsed 0-based spine index when location is an EPUB CFI, else -1
  bool notFound = false;    // True when server returned 404 (no progress saved yet)
};

// True when at least one installed plugin declares a valid "sync" section.
bool anySyncPlugin();

// True when an installed "sync" plugin matches the book's <bookPath>.meta.json sidecar.
bool hasSyncForBook(const std::string& bookPath);

// Display title of the "sync" plugin matching `bookPath`, or empty when none matches.
std::string syncTitleForBook(const std::string& bookPath);

// Pull remote progress for `bookPath` using the matching plugin's "sync.pull" template.
bool pullProgress(const std::string& bookPath, RemoteProgress& out, std::string& errorOut);

// Push local progress for `bookPath` using the matching plugin's "sync.push" template.
bool pushProgress(const std::string& bookPath, float progressFrac, const std::string& location, bool finished,
                  std::string& errorOut);

}  // namespace pluginsync
