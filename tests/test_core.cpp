#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "tse/crypto.hpp"
#include "tse/database.hpp"
#include "tse/html.hpp"
#include "tse/segmenter.hpp"
#include "tse/url.hpp"

namespace {

auto Contains(const std::vector<std::string> &values,
              const std::string &expected) -> bool {
  return std::ranges::find(values, expected) != values.end();
}

auto FreshTempDatabasePath() -> std::filesystem::path {
  const auto db_path =
      std::filesystem::temp_directory_path() / "tse_modern_test.db";
  std::filesystem::remove(db_path);
  std::filesystem::remove(std::filesystem::path(db_path.string() + "-wal"));
  std::filesystem::remove(std::filesystem::path(db_path.string() + "-shm"));
  return db_path;
}

auto MakeSearchDocument() -> tse::Document {
  const auto segmenter = tse::Segmenter{};

  auto document = tse::Document{};
  document.url = "https://example.com/";
  document.normalized_url = *tse::NormalizeUrl(document.url);
  document.title = "Modern Search";
  document.body_html =
      "<title>Modern Search</title><p>SQLite FTS5 enables 测试 search.</p>";
  document.body_text = tse::HtmlToText(document.body_html);
  document.search_text =
      segmenter.BuildSearchText(document.title + "\n" + document.body_text);
  document.status_code = 200;
  document.content_type = "text/html";
  document.checksum = tse::Sha256Hex(document.body_html);
  document.links.push_back({"https://example.com/next", "next"});
  return document;
}

void TestUrl() {
  // Arrange
  const auto raw_url = std::string{"HTTP://Example.COM:80/a/../b/?q=1#frag"};
  const auto base_url = std::string{"https://example.com/docs/page.html"};
  const auto relative_url = std::string{"../next?q=1"};

  // Act
  const auto normalized = tse::NormalizeUrl(raw_url);
  const auto resolved = tse::ResolveUrl(base_url, relative_url);

  // Assert
  assert(normalized.has_value());
  assert(*normalized == "http://example.com/b/?q=1");
  assert(resolved.has_value());
  assert(*resolved == "https://example.com/next?q=1");
}

void TestHtml() {
  // Arrange
  const auto html = std::string{
      "<html><head><title>Modern TSE</title><script>hidden()</script></head>"
      "<body><h1>Hello &amp; Search</h1><a href='/next'>Next "
      "page</a></body></html>"};

  // Act
  const auto page = tse::ExtractHtml(html);

  // Assert
  assert(page.title == "Modern TSE");
  assert(page.text.find("Hello & Search") != std::string::npos);
  assert(page.text.find("hidden") == std::string::npos);
  assert(page.links.size() == 1);
  assert(page.links[0].url == "/next");
  assert(page.links[0].anchor_text == "Next page");
}

void TestSegmenter() {
  // Arrange
  const auto segmenter = tse::Segmenter{};
  const auto input = std::string{"Modern 搜索测试"};

  // Act
  const auto terms = segmenter.Segment(input);
  const auto fts_query = segmenter.BuildFtsQuery(terms);

  // Assert
  assert(!terms.empty());
  assert(Contains(terms, "modern"));
  assert(Contains(terms, "搜索"));
  assert(fts_query.find(" OR ") != std::string::npos);
}

void TestDatabase() {
  // Arrange
  auto database = tse::Database{};
  database.Open(FreshTempDatabasePath().string());
  database.Initialize();
  const auto document = MakeSearchDocument();

  // Act
  const auto id = database.UpsertDocument(document);
  database.ReplaceLinks(id, document.links);
  const auto result = database.Search("SQLite 测试", 1, 10);
  const auto snapshot = database.SnapshotHtml("https://example.com/", "测试");
  const auto stats = database.Stats();

  // Assert
  assert(result.total == 1);
  assert(result.results.size() == 1);
  assert(result.results[0].url == "https://example.com/");
  assert(result.results[0].snippet.find("<mark>") != std::string::npos);
  assert(snapshot.find("Cached Snapshot") != std::string::npos);
  assert(snapshot.find("<mark>") != std::string::npos);
  assert(stats.documents == 1);
  assert(stats.links == 1);
}

} // namespace

auto main() -> int {
  TestUrl();
  TestHtml();
  TestSegmenter();
  TestDatabase();

  std::cout << "All tests passed\n";
  return 0;
}
