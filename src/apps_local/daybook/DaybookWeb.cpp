#include "DaybookWeb.h"

#include <Logging.h>

#ifndef SIMULATOR
#include <ESPmDNS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <string>

#include "../../util/DeviceHostname.h"
#include "DaybookWebApi.h"

namespace {

// The ESP32 WebServer as the handlers' IO.
class ServerIO final : public daybook::web::IO {
 public:
  explicit ServerIO(WebServer& server) : server_(server) {}
  std::string arg(const char* name) override { return std::string(server_.arg(name).c_str()); }
  void sendHtml(const char* page) override { server_.send(200, "text/html; charset=utf-8", page); }
  void sendJson(const int code, const std::string& body) override { server_.send(code, "application/json", body.c_str()); }
  void beginChunked(const char* contentType) override {
    server_.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server_.send(200, contentType, "");
  }
  void chunk(const char* text) override { server_.sendContent(text); }
  void endChunked() override { server_.sendContent(""); }

 private:
  WebServer& server_;
};

}  // namespace

DaybookWeb::DaybookWeb() = default;
DaybookWeb::~DaybookWeb() { stop(); }

bool DaybookWeb::begin() {
  if (running_) return true;
  server_ = std::shared_ptr<WebServer>(new (std::nothrow) WebServer(80));
  if (!server_) return false;
  WebServer& s = *server_;
  WiFi.setSleep(false);  // a sleeping radio is what makes a page "unreachable"
  const auto page = [&s] {
    ServerIO io(s);
    daybook::web::page(io);
  };
  s.on("/", HTTP_GET, page);
  s.on("/daybook", HTTP_GET, page);
  s.on("/api/daybook", HTTP_GET, [&s] {
    ServerIO io(s);
    daybook::web::get(io);
  });
  s.on("/api/daybook", HTTP_POST, [&s] {
    ServerIO io(s);
    daybook::web::post(io, std::string(s.arg("plain").c_str()));
  });
  s.onNotFound([&s] { s.send(404, "text/plain", "Not found"); });
  s.begin();
  running_ = true;
  MDNS.end();
  const bool mdnsUp = MDNS.begin(devicehost::mdnsName());
  url_ = "http://" + std::string(WiFi.localIP().toString().c_str()) + "/";
  readableUrl_ = mdnsUp ? std::string("http://") + devicehost::mdnsName() + ".local/" : url_;
  LOG_INF("DBW", "web server up");
  return true;
}

void DaybookWeb::stop() {
  if (server_) {
    server_->stop();
    server_.reset();
    MDNS.end();
  }
  running_ = false;
}

void DaybookWeb::pump() {
  if (server_ && running_) server_->handleClient();
}

#else  // SIMULATOR: no networking shim

DaybookWeb::DaybookWeb() = default;
DaybookWeb::~DaybookWeb() = default;
bool DaybookWeb::begin() { return false; }
void DaybookWeb::stop() {}
void DaybookWeb::pump() {}

#endif
