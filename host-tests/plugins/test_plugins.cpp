#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "PluginCatalogPaging.h"
#include "PluginCfi.h"

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

void testCatalogPaging() {
  using plugincatalogpaging::limitFor;
  using plugincatalogpaging::Paging;
  using plugincatalogpaging::pagingOf;

  const std::vector<std::pair<std::string, std::string>> emptyHeaders{};
  CHECK(pagingOf("", "", emptyHeaders) == Paging::Static);
  CHECK(pagingOf("https://example.com/books?page={page}", "", emptyHeaders) == Paging::PageNumber);
  CHECK(pagingOf("https://abs.example.com/api/libraries/lib1/items?limit={limit}&page={page0}", "", emptyHeaders) ==
        Paging::SizedPages);
  CHECK(pagingOf("https://abs.example.com/api/items?page={page0}", "", emptyHeaders) == Paging::PageNumber);
  CHECK(pagingOf("https://example.com/books?offset={offset}&limit={limit}", "", emptyHeaders) == Paging::Offset);

  CHECK(limitFor(Paging::SizedPages, 20) == 20);
  CHECK(limitFor(Paging::PageNumber, 20) == 21);
  CHECK(limitFor(Paging::Offset, 20) == 21);

  CHECK(plugincatalogpaging::usesListId("https://abs.example.com/api/libraries/{list_id}/search?q={query}", ""));
  CHECK(plugincatalogpaging::usesListId("https://abs.example.com/api/libraries/{list_id_enc}/search", ""));
  CHECK(!plugincatalogpaging::usesListId("https://example.com/search?q={query}", ""));
}

void testEpubCfiRoundTrip() {
  using pluginsync::buildSpineCfi;
  using pluginsync::parseSpineFromCfi;

  CHECK(buildSpineCfi(-1).empty());
  CHECK(buildSpineCfi(0) == "epubcfi(/6/2!/4/1:0)");
  CHECK(buildSpineCfi(6) == "epubcfi(/6/14!/4/1:0)");

  CHECK(parseSpineFromCfi("epubcfi(/6/2!/4/1:0)") == 0);
  CHECK(parseSpineFromCfi("epubcfi(/6/14!/4/2/1:42)") == 6);
  CHECK(parseSpineFromCfi("epubcfi(/6/13!/4/1:0)") == -1);
  CHECK(parseSpineFromCfi("not-a-cfi") == -1);
  CHECK(parseSpineFromCfi("") == -1);
}

}  // namespace

int main() {
  testCatalogPaging();
  testEpubCfiRoundTrip();

  if (g_failed != 0) {
    std::fprintf(stderr, "plugins: %d/%d checks failed\n", g_failed, g_checks);
    return 1;
  }
  std::printf("plugins: %d checks passed\n", g_checks);
  return 0;
}
