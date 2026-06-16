#include "tse/crawler.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>

#include "tse/crypto.hpp"
#include "tse/database.hpp"
#include "tse/html.hpp"
#include "tse/http_client.hpp"
#include "tse/segmenter.hpp"
#include "tse/url.hpp"

namespace tse {
namespace {

auto LoadSeeds(const std::string &path) -> std::vector<std::string> {
  auto input = std::ifstream{path};
  if (!input)
    throw std::runtime_error("cannot open seed file: " + path);

  auto seeds = std::vector<std::string>{};
  auto line = std::string{};
  while (std::getline(input, line)) {
    line.erase(0, line.find_first_not_of(" \t\r\n"));
    const auto end = line.find_last_not_of(" \t\r\n");
    if (end == std::string::npos)
      continue;
    line.erase(end + 1);
    if (line.empty() || line.starts_with('#'))
      continue;
    if (auto normalized = NormalizeUrl(line))
      seeds.push_back(*normalized);
  }
  return seeds;
}

} // namespace

auto Crawler::Run(const CrawlOptions &options) const -> CrawlSummary {
  auto database = Database{};
  database.Open(options.db_path);
  database.Initialize();

  auto queue = std::deque<std::pair<std::string, int>>{};
  auto seen = std::set<std::string>{};
  auto queue_mutex = std::mutex{};
  auto queue_changed = std::condition_variable{};
  auto active_workers = 0;
  auto stop = false;

  for (const auto &seed : LoadSeeds(options.seeds_path)) {
    queue.emplace_back(seed, 0);
    seen.insert(seed);
    database.UpsertQueue(seed, 0, "queued", "");
  }
  if (queue.empty())
    return {};

  auto started = std::atomic<int>{0};
  auto fetched = std::atomic<int>{0};
  auto failed = std::atomic<int>{0};
  auto discovered = std::atomic<int>{static_cast<int>(seen.size())};

  const auto worker_count = std::max(1, options.workers);
  auto workers = std::vector<std::jthread>{};
  workers.reserve(static_cast<std::size_t>(worker_count));

  auto finish_task = [&] -> void {
    auto lock = std::lock_guard{queue_mutex};
    --active_workers;
    if ((queue.empty() && active_workers == 0) ||
        started.load() >= options.max_pages) {
      stop = true;
    }
    queue_changed.notify_all();
  };

  for (auto worker = 0; worker < worker_count; ++worker) {
    workers.emplace_back([&, worker] -> void {
      auto worker_db = Database{};
      worker_db.Open(options.db_path);
      worker_db.Initialize();
      auto client = HttpClient{};
      auto segmenter = Segmenter{};

      while (true) {
        auto task = std::pair<std::string, int>{};
        {
          auto lock = std::unique_lock{queue_mutex};
          queue_changed.wait(lock,
                             [&] -> bool { return stop || !queue.empty(); });
          if (stop || started.load() >= options.max_pages) {
            stop = true;
            queue_changed.notify_all();
            return;
          }
          task = queue.front();
          queue.pop_front();
          ++active_workers;
          ++started;
        }

        const auto [url, depth] = task;
        worker_db.MarkQueueStatus(url, "fetching");
        const auto fetch = client.Fetch(url);
        if (!fetch.ok) {
          ++failed;
          worker_db.MarkQueueStatus(url, "failed", fetch.error);
          std::cerr << "FAILED " << url << ": " << fetch.error << "\n";
          finish_task();
          continue;
        }

        const auto extracted = ExtractHtml(fetch.body);
        auto document = Document{};
        document.url = fetch.final_url.empty() ? url : fetch.final_url;
        document.normalized_url = NormalizeUrl(document.url).value_or(url);
        document.title = extracted.title;
        document.body_html = fetch.body;
        document.body_text = extracted.text;
        document.search_text =
            segmenter.BuildSearchText(extracted.title + "\n" + extracted.text);
        document.status_code = fetch.status_code;
        document.content_type = fetch.content_type;
        document.checksum = Sha256Hex(fetch.body);

        for (const auto &link : extracted.links) {
          if (auto resolved = ResolveUrl(document.normalized_url, link.url)) {
            document.links.push_back(Link{*resolved, link.anchor_text});
            if (depth + 1 <= options.max_depth) {
              auto inserted = false;
              {
                auto lock = std::lock_guard{queue_mutex};
                inserted = seen.insert(*resolved).second;
                if (inserted)
                  queue.emplace_back(*resolved, depth + 1);
              }
              if (inserted) {
                worker_db.UpsertQueue(*resolved, depth + 1, "queued",
                                      document.normalized_url);
                ++discovered;
                queue_changed.notify_one();
              }
            }
          }
        }

        const auto document_id = worker_db.UpsertDocument(document);
        worker_db.ReplaceLinks(document_id, document.links);
        worker_db.MarkQueueStatus(url, "done");
        ++fetched;
        std::cout << "FETCHED [" << worker << "] " << document.normalized_url
                  << " (" << document.links.size() << " links)\n";
        finish_task();
      }
    });
  }

  workers.clear();

  auto summary = CrawlSummary{};
  summary.fetched = fetched.load();
  summary.failed = failed.load();
  summary.discovered = discovered.load();
  return summary;
}

} // namespace tse
