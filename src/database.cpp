#include "tse/database.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "tse/html.hpp"
#include "tse/url.hpp"

namespace tse {
namespace {

class Statement {
public:
  Statement(sqlite3 *db, const std::string &sql) : db_(db) {
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt_, nullptr) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }

  ~Statement() { sqlite3_finalize(stmt_); }

  Statement(const Statement &) = delete;
  auto operator=(const Statement &) -> Statement & = delete;

  void Bind(int index, const std::string &value) {
    if (sqlite3_bind_text(stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT) !=
        SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }

  void Bind(int index, std::int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }

  void Bind(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) {
      throw std::runtime_error(sqlite3_errmsg(db_));
    }
  }

  auto Step() -> bool {
    const auto rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW)
      return true;
    if (rc == SQLITE_DONE)
      return false;
    throw std::runtime_error(sqlite3_errmsg(db_));
  }

  void Execute() {
    while (Step()) {
    }
  }

  void Reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
  }

  [[nodiscard]] auto ColumnInt64(int index) const -> std::int64_t {
    return sqlite3_column_int64(stmt_, index);
  }

  [[nodiscard]] auto ColumnInt(int index) const -> int {
    return sqlite3_column_int(stmt_, index);
  }

  [[nodiscard]] auto ColumnDouble(int index) const -> double {
    return sqlite3_column_double(stmt_, index);
  }

  [[nodiscard]] auto ColumnText(int index) const -> std::string {
    const auto *text = sqlite3_column_text(stmt_, index);
    return text ? reinterpret_cast<const char *>(text) : "";
  }

private:
  sqlite3 *db_;
  sqlite3_stmt *stmt_ = nullptr;
};

auto NowUtc() -> std::string {
  const auto now = std::chrono::system_clock::now();
  const auto tt = std::chrono::system_clock::to_time_t(now);
  auto tm = std::tm{};
  gmtime_r(&tt, &tm);

  auto out = std::ostringstream{};
  out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

} // namespace

Database::Database() = default;

Database::~Database() {
  if (db_)
    sqlite3_close(db_);
}

Database::Database(Database &&other) noexcept : db_(other.db_) {
  other.db_ = nullptr;
}

auto Database::operator=(Database &&other) noexcept -> Database & {
  if (this == &other)
    return *this;
  if (db_)
    sqlite3_close(db_);
  db_ = other.db_;
  other.db_ = nullptr;
  return *this;
}

void Database::Open(const std::string &path) {
  if (db_)
    sqlite3_close(db_);
  db_ = nullptr;

  const auto db_path = std::filesystem::path(path);
  if (db_path.has_parent_path()) {
    std::filesystem::create_directories(db_path.parent_path());
  }

  if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
    auto error = db_ ? std::string{sqlite3_errmsg(db_)}
                     : std::string{"cannot allocate sqlite handle"};
    if (db_)
      sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error(error);
  }
  sqlite3_busy_timeout(db_, 5000);
  Exec("PRAGMA foreign_keys = ON;");
  Exec("PRAGMA journal_mode = WAL;");
}

void Database::Initialize() {
  Exec(R"SQL(
CREATE TABLE IF NOT EXISTS documents(
  id INTEGER PRIMARY KEY,
  url TEXT NOT NULL UNIQUE,
  normalized_url TEXT NOT NULL UNIQUE,
  title TEXT,
  body_html TEXT,
  body_text TEXT,
  search_text TEXT NOT NULL,
  status_code INTEGER,
  content_type TEXT,
  checksum TEXT,
  fetched_at TEXT NOT NULL
);

CREATE VIRTUAL TABLE IF NOT EXISTS documents_fts USING fts5(
  title,
  url,
  search_text,
  content='documents',
  content_rowid='id',
  tokenize='unicode61'
);

CREATE TABLE IF NOT EXISTS links(
  id INTEGER PRIMARY KEY,
  src_doc_id INTEGER NOT NULL,
  dst_url TEXT NOT NULL,
  anchor_text TEXT,
  FOREIGN KEY(src_doc_id) REFERENCES documents(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS crawl_queue(
  url TEXT PRIMARY KEY,
  depth INTEGER NOT NULL,
  status TEXT NOT NULL,
  discovered_from TEXT,
  last_error TEXT
);

CREATE TABLE IF NOT EXISTS meta(
  key TEXT PRIMARY KEY,
  value TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_links_src_doc_id ON links(src_doc_id);
CREATE INDEX IF NOT EXISTS idx_crawl_queue_status ON crawl_queue(status);

CREATE TRIGGER IF NOT EXISTS documents_ai AFTER INSERT ON documents BEGIN
  INSERT INTO documents_fts(rowid, title, url, search_text)
  VALUES (new.id, new.title, new.url, new.search_text);
END;

CREATE TRIGGER IF NOT EXISTS documents_ad AFTER DELETE ON documents BEGIN
  INSERT INTO documents_fts(documents_fts, rowid, title, url, search_text)
  VALUES('delete', old.id, old.title, old.url, old.search_text);
END;

CREATE TRIGGER IF NOT EXISTS documents_au AFTER UPDATE ON documents BEGIN
  INSERT INTO documents_fts(documents_fts, rowid, title, url, search_text)
  VALUES('delete', old.id, old.title, old.url, old.search_text);
  INSERT INTO documents_fts(rowid, title, url, search_text)
  VALUES (new.id, new.title, new.url, new.search_text);
END;

INSERT INTO meta(key, value) VALUES('schema_version', '1')
ON CONFLICT(key) DO UPDATE SET value = excluded.value;
)SQL");
}

void Database::Vacuum() { Exec("VACUUM;"); }

auto Database::UpsertDocument(const Document &document) -> std::int64_t {
  auto transaction = Transaction(*this);

  auto upsert = Statement(db_, R"SQL(
INSERT INTO documents(
  url, normalized_url, title, body_html, body_text, search_text,
  status_code, content_type, checksum, fetched_at
) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
ON CONFLICT(normalized_url) DO UPDATE SET
  url = excluded.url,
  title = excluded.title,
  body_html = excluded.body_html,
  body_text = excluded.body_text,
  search_text = excluded.search_text,
  status_code = excluded.status_code,
  content_type = excluded.content_type,
  checksum = excluded.checksum,
  fetched_at = excluded.fetched_at;
)SQL");
  upsert.Bind(1, document.url);
  upsert.Bind(2, document.normalized_url);
  upsert.Bind(3, document.title);
  upsert.Bind(4, document.body_html);
  upsert.Bind(5, document.body_text);
  upsert.Bind(6, document.search_text);
  upsert.Bind(7, document.status_code);
  upsert.Bind(8, document.content_type);
  upsert.Bind(9, document.checksum);
  upsert.Bind(10, document.fetched_at.empty() ? NowUtc() : document.fetched_at);
  upsert.Execute();

  auto select =
      Statement(db_, "SELECT id FROM documents WHERE normalized_url = ?;");
  select.Bind(1, document.normalized_url);
  if (!select.Step())
    throw std::runtime_error("document upsert did not return an id");
  const auto id = select.ColumnInt64(0);

  transaction.Commit();
  return id;
}

void Database::ReplaceLinks(std::int64_t source_document_id,
                            const std::vector<Link> &links) {
  auto transaction = Transaction(*this);
  auto remove = Statement(db_, "DELETE FROM links WHERE src_doc_id = ?;");
  remove.Bind(1, source_document_id);
  remove.Execute();

  auto insert = Statement(
      db_,
      "INSERT INTO links(src_doc_id, dst_url, anchor_text) VALUES (?, ?, ?);");
  for (const auto &link : links) {
    insert.Bind(1, source_document_id);
    insert.Bind(2, link.url);
    insert.Bind(3, link.anchor_text);
    insert.Execute();
    insert.Reset();
  }
  transaction.Commit();
}

void Database::UpsertQueue(const std::string &url, int depth,
                           const std::string &status,
                           const std::string &discovered_from,
                           const std::string &error) {
  auto stmt = Statement(db_, R"SQL(
INSERT INTO crawl_queue(url, depth, status, discovered_from, last_error)
VALUES (?, ?, ?, ?, ?)
ON CONFLICT(url) DO NOTHING;
)SQL");
  stmt.Bind(1, url);
  stmt.Bind(2, depth);
  stmt.Bind(3, status);
  stmt.Bind(4, discovered_from);
  stmt.Bind(5, error);
  stmt.Execute();
}

void Database::MarkQueueStatus(const std::string &url,
                               const std::string &status,
                               const std::string &error) {
  auto stmt = Statement(
      db_, "UPDATE crawl_queue SET status = ?, last_error = ? WHERE url = ?;");
  stmt.Bind(1, status);
  stmt.Bind(2, error);
  stmt.Bind(3, url);
  stmt.Execute();
}

auto Database::Search(const std::string &query, int page, int limit) const
    -> SearchResponse {
  const auto start = std::chrono::steady_clock::now();
  auto response = SearchResponse{};
  response.query = query;
  response.terms = segmenter_.Segment(query);

  if (page < 1)
    page = 1;
  if (limit < 1)
    limit = 20;
  const auto fts_query = segmenter_.BuildFtsQuery(response.terms);
  if (fts_query.empty())
    return response;

  {
    auto count = Statement(
        db_, "SELECT count(*) FROM documents_fts WHERE documents_fts MATCH ?;");
    count.Bind(1, fts_query);
    if (count.Step())
      response.total = count.ColumnInt(0);
  }

  auto stmt = Statement(db_, R"SQL(
SELECT d.id, d.url, COALESCE(d.title, ''), COALESCE(d.body_text, ''),
       length(COALESCE(d.body_html, '')), bm25(documents_fts) AS rank
FROM documents_fts
JOIN documents d ON d.id = documents_fts.rowid
WHERE documents_fts MATCH ?
ORDER BY rank
LIMIT ? OFFSET ?;
)SQL");
  stmt.Bind(1, fts_query);
  stmt.Bind(2, limit);
  stmt.Bind(3, (page - 1) * limit);

  while (stmt.Step()) {
    auto result = SearchResult{};
    result.id = stmt.ColumnInt64(0);
    result.url = stmt.ColumnText(1);
    result.title = stmt.ColumnText(2);
    const auto body_text = stmt.ColumnText(3);
    result.size = stmt.ColumnInt64(4);
    result.score = -stmt.ColumnDouble(5);
    result.snippet = BuildSnippet(body_text, response.terms);
    response.results.push_back(std::move(result));
  }

  const auto end = std::chrono::steady_clock::now();
  response.time_ms =
      std::chrono::duration<double, std::milli>(end - start).count();
  return response;
}

auto Database::SnapshotHtml(const std::string &url,
                            const std::string &query) const -> std::string {
  const auto normalized = NormalizeUrl(url).value_or(url);
  auto stmt =
      Statement(db_, "SELECT url, COALESCE(body_text, '') FROM documents WHERE "
                     "normalized_url = ? OR url = ? LIMIT 1;");
  stmt.Bind(1, normalized);
  stmt.Bind(2, url);
  if (!stmt.Step()) {
    return "<!DOCTYPE html><html><body><h2>Snapshot not "
           "available</h2></body></html>";
  }

  const auto stored_url = stmt.ColumnText(0);
  const auto body = stmt.ColumnText(1);
  const auto terms = segmenter_.Segment(query);

  return "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
         "<title>Snapshot</title>"
         "<style>body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;"
         "max-width:900px;"
         "margin:24px auto;padding:0 20px;line-height:1.75;color:#222}"
         "mark{background:#ffeb3b;color:#111;padding:0 2px}"
         ".url{color:#0969da;word-break:break-all;border-bottom:1px solid "
         "#ddd;padding-bottom:12px;margin-bottom:18px}"
         "</style></head><body><h2>Cached Snapshot</h2><div class='url'>" +
         EscapeHtml(stored_url) + "</div><div>" + HighlightTerms(body, terms) +
         "</div></body></html>";
}

auto Database::Stats() const -> DatabaseStats {
  auto stats = DatabaseStats{};
  stats.documents = ScalarInt("SELECT count(*) FROM documents;");
  stats.links = ScalarInt("SELECT count(*) FROM links;");
  stats.queued =
      ScalarInt("SELECT count(*) FROM crawl_queue WHERE status = 'queued';");
  stats.done =
      ScalarInt("SELECT count(*) FROM crawl_queue WHERE status = 'done';");
  stats.failed =
      ScalarInt("SELECT count(*) FROM crawl_queue WHERE status = 'failed';");
  return stats;
}

void Database::Exec(const std::string &sql) const {
  auto *error = static_cast<char *>(nullptr);
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    auto message =
        error ? std::string{error} : std::string{"sqlite exec failed"};
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

auto Database::ScalarInt(const std::string &sql) const -> int {
  auto stmt = Statement(db_, sql);
  if (!stmt.Step())
    return 0;
  return stmt.ColumnInt(0);
}

Transaction::Transaction(const Database &database) : db_(database.Raw()) {
  auto *error = static_cast<char *>(nullptr);
  if (sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, &error) !=
      SQLITE_OK) {
    auto message =
        error ? std::string{error} : std::string{"begin transaction failed"};
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

Transaction::~Transaction() {
  if (!committed_) {
    sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
  }
}

void Transaction::Commit() {
  auto *error = static_cast<char *>(nullptr);
  if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &error) != SQLITE_OK) {
    auto message =
        error ? std::string{error} : std::string{"commit transaction failed"};
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
  committed_ = true;
}

} // namespace tse
