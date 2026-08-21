#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "dstore/common/lru_cache.h"
#include "dstore/common/result.h"

namespace dstore {

struct ChunkRef {
  std::string checksum;
  std::uint64_t size = 0;
};

class ChunkStore {
 public:
  ChunkStore(std::filesystem::path root, std::size_t cache_bytes);
  Result<ChunkRef> put(const std::vector<std::uint8_t>& bytes);
  Result<std::vector<std::uint8_t>> get(const std::string& checksum);
  Result<void> remove_if_unreferenced(const std::string& checksum, int references);
  bool exists(const std::string& checksum) const;

 private:
  std::filesystem::path path_for(const std::string& checksum) const;
  std::filesystem::path root_;
  mutable std::mutex mu_;
  LruCache<std::string, std::vector<std::uint8_t>> cache_;
};

std::vector<std::vector<std::uint8_t>> chunk_bytes(const std::vector<std::uint8_t>& input, std::size_t chunk_size);

}  // namespace dstore
