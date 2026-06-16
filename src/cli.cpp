#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "tse/crawler.hpp"
#include "tse/database.hpp"
#include "tse/simple_server.hpp"

namespace {

constexpr int kDefaultLimit = 20;

auto OptionValue(const std::vector<std::string> &args, const std::string &name,
                 const std::string &fallback) -> std::string {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == name)
      return args[i + 1];
  }
  return fallback;
}

auto OptionInt(const std::vector<std::string> &args, const std::string &name,
               int fallback) -> int {
  const auto value = OptionValue(args, name, "");
  if (value.empty())
    return fallback;
  return std::stoi(value);
}

auto PositionalQuery(const std::vector<std::string> &args) -> std::string {
  auto query = std::ostringstream{};
  for (auto i = std::size_t{0}; i < args.size(); ++i) {
    if (args[i].starts_with("--")) {
      ++i;
      continue;
    }
    if (query.tellp() > 0)
      query << ' ';
    query << args[i];
  }
  return query.str();
}

auto JsonEscape(std::string_view value) -> std::string {
  auto out = std::string{};
  for (const auto c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
      break;
    }
  }
  return out;
}

auto SearchResponseJson(const tse::SearchResponse &result) -> std::string {
  auto json = std::ostringstream{};
  json << "{\n";
  json << "  \"query\": \"" << JsonEscape(result.query) << "\",\n";
  json << "  \"total\": " << result.total << ",\n";
  json << "  \"time_ms\": " << result.time_ms << ",\n";
  json << "  \"terms\": [";
  for (auto i = std::size_t{0}; i < result.terms.size(); ++i) {
    if (i > 0)
      json << ", ";
    json << "\"" << JsonEscape(result.terms[i]) << "\"";
  }
  json << "],\n";
  json << "  \"results\": [\n";
  for (auto i = std::size_t{0}; i < result.results.size(); ++i) {
    const auto &item = result.results[i];
    if (i > 0)
      json << ",\n";
    json << "    {";
    json << "\"url\": \"" << JsonEscape(item.url) << "\", ";
    json << "\"title\": \"" << JsonEscape(item.title) << "\", ";
    json << "\"snippet\": \"" << JsonEscape(item.snippet) << "\", ";
    json << "\"size\": " << item.size << ", ";
    json << "\"score\": " << item.score;
    json << "}";
  }
  json << "\n  ]\n";
  json << "}\n";
  return json.str();
}

void PrintUsage(const char *program) {
  std::cerr << "Usage:\n"
            << "  " << program << " init --db Data/tse.db\n"
            << "  " << program
            << " crawl --db Data/tse.db --seeds tse/seed --max-pages 1000 "
               "--workers 10 "
               "[--max-depth 2]\n"
            << "  " << program
            << " search --db Data/tse.db \"query text\" --page 1 --limit 20\n"
            << "  " << program
            << " serve --db Data/tse.db --web web --port 8888\n"
            << "  " << program << " stats --db Data/tse.db\n"
            << "  " << program << " vacuum --db Data/tse.db\n";
}

} // namespace

auto main(int argc, char *argv[]) -> int {
  if (argc < 2) {
    PrintUsage(argv[0]);
    return 1;
  }

  try {
    const auto command = std::string{argv[1]};
    auto args = std::vector<std::string>{};
    for (int i = 2; i < argc; ++i)
      args.emplace_back(argv[i]);

    const auto db_path = OptionValue(args, "--db", "Data/tse.db");

    if (command == "init") {
      auto database = tse::Database{};
      database.Open(db_path);
      database.Initialize();
      std::cout << "Initialized " << db_path << "\n";
      return 0;
    }

    if (command == "crawl") {
      auto options = tse::CrawlOptions{};
      options.db_path = db_path;
      options.seeds_path = OptionValue(args, "--seeds", "tse/seed");
      options.max_pages = OptionInt(args, "--max-pages", 1000);
      options.workers = OptionInt(args, "--workers", 10);
      options.max_depth = OptionInt(args, "--max-depth", 2);
      const auto summary = tse::Crawler().Run(options);
      std::cout << "Fetched: " << summary.fetched << "\n";
      std::cout << "Failed: " << summary.failed << "\n";
      std::cout << "Discovered: " << summary.discovered << "\n";
      return 0;
    }

    if (command == "search") {
      auto database = tse::Database{};
      database.Open(db_path);
      database.Initialize();
      const auto query = PositionalQuery(args);
      const auto page = OptionInt(args, "--page", 1);
      const auto limit = OptionInt(args, "--limit", kDefaultLimit);
      const auto result = database.Search(query, page, limit);
      for (const auto &item : result.results) {
        std::cout << item.title << "\n"
                  << item.url << "\n"
                  << item.snippet << "\n\n";
      }
      std::cout << "Total: " << result.total << " (" << result.time_ms
                << " ms)\n";
      return 0;
    }

    if (command == "serve") {
      auto database = tse::Database{};
      database.Open(db_path);
      database.Initialize();

      const auto port =
          static_cast<std::uint16_t>(OptionInt(args, "--port", 8888));
      const auto web_root = OptionValue(args, "--web", "web");
      auto server = tse::SimpleServer{port, web_root};

      server.AddRoute(
          "/api/search",
          [&](const tse::HttpRequest &req, tse::HttpResponse &res) -> void {
            const auto query =
                req.params.contains("q") ? req.params.at("q") : std::string{};
            const auto page = req.params.contains("p")
                                  ? std::max(1, std::stoi(req.params.at("p")))
                                  : 1;
            const auto result = database.Search(query, page, kDefaultLimit);
            res.content_type = "application/json; charset=utf-8";
            res.body = SearchResponseJson(result);
          });

      server.AddRoute(
          "/api/snapshot",
          [&](const tse::HttpRequest &req, tse::HttpResponse &res) -> void {
            const auto url = req.params.contains("url") ? req.params.at("url")
                                                        : std::string{};
            const auto word = req.params.contains("word")
                                  ? req.params.at("word")
                                  : std::string{};
            res.content_type = "text/html; charset=utf-8";
            res.body = database.SnapshotHtml(url, word);
          });

      server.Run();
      return 0;
    }

    if (command == "stats") {
      auto database = tse::Database{};
      database.Open(db_path);
      database.Initialize();
      const auto stats = database.Stats();
      std::cout << "Documents: " << stats.documents << "\n";
      std::cout << "Links: " << stats.links << "\n";
      std::cout << "Queued: " << stats.queued << "\n";
      std::cout << "Done: " << stats.done << "\n";
      std::cout << "Failed: " << stats.failed << "\n";
      return 0;
    }

    if (command == "vacuum") {
      auto database = tse::Database{};
      database.Open(db_path);
      database.Vacuum();
      std::cout << "Vacuumed " << db_path << "\n";
      return 0;
    }

    PrintUsage(argv[0]);
    return 1;
  } catch (const std::exception &ex) {
    std::cerr << "Error: " << ex.what() << "\n";
    return 1;
  }
}
