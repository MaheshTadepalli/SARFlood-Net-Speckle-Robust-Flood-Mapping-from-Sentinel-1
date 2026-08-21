#pragma once

#include <string>
#include <vector>

#include "dstore/common/result.h"
#include "dstore/storage/chunk_store.h"
#include "dstore/storage/consistent_hash.h"

namespace dstore {

class NodeClient {
 public:
  explicit NodeClient(std::string bearer_token);
  Result<void> put_chunk(const StorageNode& node, const ChunkRef& ref, const std::vector<std::uint8_t>& bytes) const;
  Result<std::vector<std::uint8_t>> get_chunk(const StorageNode& node, const std::string& checksum) const;
  Result<void> delete_chunk(const StorageNode& node, const std::string& checksum) const;
  Result<void> heartbeat(const StorageNode& node, const std::string& from_node_id) const;

 private:
  std::string token_;
};

StorageNode parse_node(std::string spec);

}  // namespace dstore
