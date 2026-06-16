#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "tse/segmenter.hpp"
#include "tse/types.hpp"

namespace tse {

class Database {
public:
  Database();
  ~Database();

  Database(const Database &) = delete;
  auto operator=(const Database &) -> Database & = delete;
  Database(Database &&) noexcept;
  auto operator=(Database &&) noexcept -> Database &;

  void Open(const std::string &path);
  void Initialize();
  void Vacuum();

  auto UpsertDocument(const Document &document) -> std::int64_t;
  void ReplaceLinks(std::int64_t source_document_id,
                    const std::vector<Link> &links);

  void UpsertQueue(const std::string &url, int depth, const std::string &status,
                   const std::string &discovered_from,
                   const std::string &error = {});
  void MarkQueueStatus(const std::string &url, const std::string &status,
                       const std::string &error = {});

  [[nodiscard]] auto Search(const std::string &query, int page, int limit) const
      -> SearchResponse;
  [[nodiscard]] auto SnapshotHtml(const std::string &url,
                                  const std::string &query) const
      -> std::string;
  [[nodiscard]] auto Stats() const -> DatabaseStats;

  [[nodiscard]] auto Raw() const -> sqlite3 * { return db_; }

private:
  sqlite3 *db_ = nullptr;
  Segmenter segmenter_;

  void Exec(const std::string &sql) const;
  [[nodiscard]] auto ScalarInt(const std::string &sql) const -> int;
};

class Transaction {
public:
  explicit Transaction(const Database &database);
  ~Transaction();

  Transaction(const Transaction &) = delete;
  auto operator=(const Transaction &) -> Transaction & = delete;

  void Commit();

private:
  sqlite3 *db_;
  bool committed_ = false;
};

} // namespace tse
