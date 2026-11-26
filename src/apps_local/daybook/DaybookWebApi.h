// Daybook's web page and API, as plain functions over a small server interface.
// Freestanding apart from ArduinoJson and the SD card, so the logic does not
// care which web server calls it (DaybookWeb.cpp wraps the ESP32 one).
#pragma once

#include <string>

namespace daybook::web {

// What a handler needs from whatever server it runs under.
class IO {
 public:
  virtual ~IO() = default;
  virtual std::string arg(const char* name) = 0;  // a query argument, "" when absent
  virtual void sendHtml(const char* page) = 0;
  virtual void sendJson(int code, const std::string& body) = 0;
  // A reply of unknown length, in pieces: begin, chunk..., end.
  virtual void beginChunked(const char* contentType) = 0;
  virtual void chunk(const char* text) = 0;
  virtual void endChunked() = 0;
};

void page(IO& io);                         // GET /
void get(IO& io);                          // GET /api/daybook?from=&to=
void post(IO& io, const std::string& body);  // POST /api/daybook, a JSON command

}  // namespace daybook::web
