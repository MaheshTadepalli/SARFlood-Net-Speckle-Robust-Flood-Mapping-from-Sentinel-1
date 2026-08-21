#include "dstore/metadata/metadata_store.h"

#include <sstream>
#include <utility>

namespace dstore {

MetadataStore::MetadataStore(std::filesystem::path db_path) : db_path_(std::move(db_path)) {}

MetadataStore::~MetadataStore() {
  if (db_) sqlite3_close(db_);
}

Result<void> MetadataStore::open() {
  std::filesystem::create_directories(db_path_.parent_path());
  if (sqlite3_open(db_path_.string().c_str(), &db_) != SQLITE_OK) return Status::Internal(sqlite3_errmsg(db_));
  return exec(
      "PRAGMA journal_mode=WAL;"
      "CREATE TABLE IF NOT EXISTS objects(bucket TEXT NOT NULL,key TEXT NOT NULL,size INTEGER NOT NULL,checksum TEXT NOT NULL,created_at INTEGER DEFAULT (unixepoch()),PRIMARY KEY(bucket,key));"
      "CREATE TABLE IF NOT EXISTS chunks(bucket TEXT NOT NULL,key TEXT NOT NULL,position INTEGER NOT NULL,checksum TEXT NOT NULL,size INTEGER NOT NULL,PRIMARY KEY(bucket,key,position));"
      "CREATE INDEX IF NOT EXISTS idx_chunks_checksum ON chunks(checksum);"
      "CREATE INDEX IF NOT EXISTS idx_objects_bucket ON objects(bucket);");
}

Result<void> MetadataStore::exec(const std::string& sql) {
  char* err = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : "sqlite error";
    sqlite3_free(err);
    return Status::Internal(msg);
  }
  return {};
}

Result<void> MetadataStore::put_object(const ObjectMetadata& m) {
  auto tx = exec("BEGIN IMMEDIATE;");
  if (!tx.ok()) return tx;
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "INSERT OR REPLACE INTO objects(bucket,key,size,checksum) VALUES(?,?,?,?)", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, m.bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, m.key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(m.size));
  sqlite3_bind_text(stmt, 4, m.checksum.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    std::string msg = sqlite3_errmsg(db_);
    sqlite3_finalize(stmt);
    exec("ROLLBACK;");
    return Status::Internal(msg);
  }
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "DELETE FROM chunks WHERE bucket=? AND key=?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, m.bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, m.key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "INSERT INTO chunks(bucket,key,position,checksum,size) VALUES(?,?,?,?,?)", -1, &stmt, nullptr);
  int pos = 0;
  for (const auto& c : m.chunks) {
    sqlite3_reset(stmt);
    sqlite3_bind_text(stmt, 1, m.bucket.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, m.key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, pos++);
    sqlite3_bind_text(stmt, 4, c.checksum.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(c.size));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
      std::string msg = sqlite3_errmsg(db_);
      sqlite3_finalize(stmt);
      exec("ROLLBACK;");
      return Status::Internal(msg);
    }
  }
  sqlite3_finalize(stmt);
  return exec("COMMIT;");
}

Result<ObjectMetadata> MetadataStore::get_object(const std::string& bucket, const std::string& key) {
  ObjectMetadata out;
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "SELECT size,checksum FROM objects WHERE bucket=? AND key=?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return Status::NotFound("object not found");
  }
  out.bucket = bucket;
  out.key = key;
  out.size = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
  out.checksum = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "SELECT checksum,size FROM chunks WHERE bucket=? AND key=? ORDER BY position", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.chunks.push_back({reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1))});
  }
  sqlite3_finalize(stmt);
  return out;
}

Result<void> MetadataStore::delete_object(const std::string& bucket, const std::string& key) {
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "DELETE FROM chunks WHERE bucket=? AND key=?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  sqlite3_prepare_v2(db_, "DELETE FROM objects WHERE bucket=? AND key=?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, bucket.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  auto changed = sqlite3_changes(db_);
  sqlite3_finalize(stmt);
  if (changed == 0) return Status::NotFound("object not found");
  return {};
}

Result<int> MetadataStore::chunk_ref_count(const std::string& checksum) {
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "SELECT COUNT(*) FROM chunks WHERE checksum=?", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, checksum.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt);
  int count = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  return count;
}

Result<std::vector<ObjectMetadata>> MetadataStore::list_objects(const std::string& bucket) {
  std::vector<ObjectMetadata> out;
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db_, "SELECT key,size,checksum FROM objects WHERE bucket=? ORDER BY key", -1, &stmt, nullptr);
  sqlite3_bind_text(stmt, 1, bucket.c_str(), -1, SQLITE_TRANSIENT);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back({bucket, reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)), {}});
  }
  sqlite3_finalize(stmt);
  return out;
}

}  // namespace dstore
