#pragma once

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "dstore/common/result.h"
#include "dstore/storage/chunk_store.h"
#include "dstore/storage/consistent_hash.h"

namespace dstore {

struct HandoffHint {
  std::string target_node_id;
  std::string target_address;
  std::string checksum;
  std::uint64_t size = 0;
};

class HintedHandoffQueue {
 public:
  explicit HintedHandoffQueue(std::filesystem::path path);
  Result<void> enqueue(const StorageNode& target, const ChunkRef& chunk);
  Result<std::vector<HandoffHint>> load() const;
  Result<void> replace(const std::vector<HandoffHint>& hints);

 private:
  std::filesystem::path path_;
  mutable std::mutex mu_;
};

}  // namespace dstore
