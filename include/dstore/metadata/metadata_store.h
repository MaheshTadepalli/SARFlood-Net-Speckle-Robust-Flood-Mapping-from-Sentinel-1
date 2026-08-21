#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include <sqlite3.h>

#include "dstore/common/result.h"
#include "dstore/storage/chunk_store.h"

namespace dstore {

struct ObjectMetadata {
  std::string bucket;
  std::string key;
  std::uint64_t size = 0;
  std::string checksum;
  std::vector<ChunkRef> chunks;
};

class MetadataStore {
 public:
  explicit MetadataStore(std::filesystem::path db_path);
  ~MetadataStore();
  MetadataStore(const MetadataStore&) = delete;
  MetadataStore& operator=(const MetadataStore&) = delete;

  Result<void> open();
  Result<void> put_object(const ObjectMetadata& metadata);
  Result<ObjectMetadata> get_object(const std::string& bucket, const std::string& key);
  Result<void> delete_object(const std::string& bucket, const std::string& key);
  Result<int> chunk_ref_count(const std::string& checksum);
  Result<std::vector<ObjectMetadata>> list_objects(const std::string& bucket);

 private:
  Result<void> exec(const std::string& sql);
  sqlite3* db_{nullptr};
  std::filesystem::path db_path_;
};

}  // namespace dstore
