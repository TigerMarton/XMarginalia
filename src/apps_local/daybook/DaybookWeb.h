// The web server behind Daybook's "Web" screen: one page, one API, on port 80,
// for as long as that screen is open. The handlers are DaybookWebApi.cpp.
#pragma once

#include <memory>
#include <string>

class WebServer;

class DaybookWeb {
 public:
  DaybookWeb();
  ~DaybookWeb();
  bool begin();   // false when it could not start
  void stop();
  void pump();    // serve what is waiting; call from loop()
  bool running() const { return running_; }
  // Where to point a browser: the numeric address always works; the .local
  // name is shown when the responder started.
  const std::string& url() const { return url_; }
  const std::string& readableUrl() const { return readableUrl_; }

 private:
  std::shared_ptr<WebServer> server_;
  bool running_ = false;
  std::string url_;
  std::string readableUrl_;
};
