#include <cstdio>
#include <cstdlib>
#include <string>

#include "CompanionCore.h"

namespace {

int g_checks = 0;
int g_failed = 0;

void check(const bool cond, const char* expr, const int line) {
  ++g_checks;
  if (!cond) {
    std::fprintf(stderr, "FAIL line %d: %s\n", line, expr);
    ++g_failed;
  }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

void testConfigParsing() {
  companion::CompanionConfig cfg;
  const char* raw = R"({
    "enabled": true,
    "host": "https://companion.example.com/",
    "api_key": "secret-token-123"
  })";
  CHECK(companion::parseConfigJson(raw, cfg));
  CHECK(cfg.enabled);
  CHECK(cfg.host == "companion.example.com");
  CHECK(cfg.apiKey == "secret-token-123");
  CHECK(cfg.isValid());

  companion::CompanionConfig disabledCfg;
  CHECK(!companion::parseConfigJson(R"({"enabled": false, "host": "companion.example.com"})", disabledCfg));
  CHECK(!disabledCfg.isValid());

  companion::CompanionConfig urlWithPathCfg;
  CHECK(companion::parseConfigJson(
      R"({"url": "https://companion.example.com/api/ereader/v1/query", "api_key": "k"})", urlWithPathCfg));
  CHECK(urlWithPathCfg.host == "companion.example.com");
  CHECK(!urlWithPathCfg.useHttp);
  CHECK(urlWithPathCfg.baseUrl() == "https://companion.example.com");

  companion::CompanionConfig httpLanCfg;
  CHECK(companion::parseConfigJson(R"({"url": "http://192.168.1.50:8095", "api_key": "k"})", httpLanCfg));
  CHECK(httpLanCfg.host == "192.168.1.50:8095");
  CHECK(httpLanCfg.useHttp);
  CHECK(httpLanCfg.baseUrl() == "http://192.168.1.50:8095");
}

void testAnchorExtraction() {
  const std::string pageText =
      "Kaladin Stormblessed stood at the edge of the shattered chasm.\n\n"
      "Syl zipped through the air beside him, her form shifting into a tiny ribbon of light "
      "as the highstorm approached from the east.";
  const std::string anchor = companion::extractPageAnchorText(pageText, 95);
  CHECK(!anchor.empty());
  CHECK(anchor.size() <= 95);
  CHECK(anchor.find("highstorm approached from the east.") != std::string::npos);
  CHECK(anchor.find('\n') == std::string::npos);
}

void testQuerySerializationAndResponseParsing() {
  companion::CompanionQuery q;
  q.bookUuid = "urn:uuid:1234-5678";
  q.isbn = "9780765326355";
  q.koMd5 = "aabbccddeeff00112233445566778899";
  q.title = "The Way of Kings";
  q.author = "Brandon Sanderson";
  q.filename = "way_of_kings.epub";
  q.spineIndex = 4;
  q.chapterTitle = "Chapter 4: The Shattered Plains";
  q.visibleOffset = 1820;
  q.hasVisibleOffset = true;
  q.chapterProgress = 0.42f;
  q.bookProgress = 0.125f;
  q.anchorText = "Syl zipped through the air beside him";
  q.selectedText = "Syl";
  q.actionType = "who_is";

  const std::string body = companion::buildQueryJson(q);
  CHECK(body.find("\"book_uuid\":\"urn:uuid:1234-5678\"") != std::string::npos);
  CHECK(body.find("\"isbn\":\"9780765326355\"") != std::string::npos);
  CHECK(body.find("\"spine_index\":4") != std::string::npos);
  CHECK(body.find("\"visible_offset\":1820") != std::string::npos);
  CHECK(body.find("\"selected_text\":\"Syl\"") != std::string::npos);
  CHECK(body.find("\"action_type\":\"who_is\"") != std::string::npos);

  companion::CompanionResponse resp;
  CHECK(companion::parseQueryResponse(
      R"json({"ans":"Syl is a windspren who has begun following Kaladin.","pos":"Ch 4 (42%)"})json", resp));
  CHECK(resp.ok);
  CHECK(resp.answer == "Syl is a windspren who has begun following Kaladin.");
  CHECK(resp.positionLabel == "Ch 4 (42%)");

  companion::CompanionResponse errResp;
  CHECK(!companion::parseQueryResponse(R"({"error":"Book not yet indexed on Companion Bridge."})", errResp));
  CHECK(!errResp.ok);
  CHECK(errResp.errorMessage == "Book not yet indexed on Companion Bridge.");
}

}  // namespace

int main() {
  testConfigParsing();
  testAnchorExtraction();
  testQuerySerializationAndResponseParsing();

  if (g_failed != 0) {
    std::fprintf(stderr, "companion: %d/%d checks failed\n", g_failed, g_checks);
    return 1;
  }
  std::printf("companion: %d checks passed\n", g_checks);
  return 0;
}
