#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tse {

struct Link {
  std::string url;
  std::string anchor_text;
};

struct Document {
  std::int64_t id = 0;
  std::string url;
  std::string normalized_url;
  std::string title;
  std::string body_html;
  std::string body_text;
  std::string search_text;
  int status_code = 0;
  std::string content_type;
  std::string checksum;
  std::string fetched_at;
  std::vector<Link> links;
};

struct SearchResult {
  std::int64_t id = 0;
  std::string url;
  std::string title;
  std::string snippet;
  std::int64_t size = 0;
  double score = 0.0;
};

struct SearchResponse {
  std::string query;
  std::vector<std::string> terms;
  std::vector<SearchResult> results;
  int total = 0;
  double time_ms = 0.0;
};

struct CrawlOptions {
  std::string db_path = "Data/tse.db";
  std::string seeds_path = "tse/seed";
  int max_pages = 1000;
  int workers = 10;
  int max_depth = 2;
};

struct CrawlSummary {
  int fetched = 0;
  int failed = 0;
  int discovered = 0;
};

struct DatabaseStats {
  int documents = 0;
  int links = 0;
  int queued = 0;
  int done = 0;
  int failed = 0;
};

} // namespace tse
