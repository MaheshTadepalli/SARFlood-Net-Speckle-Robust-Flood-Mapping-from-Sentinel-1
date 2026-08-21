#pragma once

#include <memory>
#include <vector>

#include "dstore/cluster/hinted_handoff.h"
#include "dstore/cluster/node_client.h"
#include "dstore/storage/chunk_store.h"
#include "dstore/storage/replication.h"

namespace dstore {

class DistributedChunkService {
 public:
  DistributedChunkService(ChunkStore& local, ClusterState& cluster, NodeClient client, StorageNode self,
                          int replication_factor, int write_quorum, int read_quorum, HintedHandoffQueue* handoff = nullptr,
                          std::vector<StorageNode> seed_members = {});

  Result<ChunkRef> put(const std::vector<std::uint8_t>& bytes);
  Result<std::vector<std::uint8_t>> get(const std::string& checksum);
  Result<void> remove(const std::string& checksum, int references);
  Result<int> replay_hinted_handoff();

 private:
  bool is_self(const StorageNode& node) const;
  void refresh_seed_members();
  ChunkStore& local_;
  ClusterState& cluster_;
  NodeClient client_;
  StorageNode self_;
  std::vector<StorageNode> seed_members_;
  int replication_factor_;
  int write_quorum_;
  int read_quorum_;
  HintedHandoffQueue* handoff_;
};

}  // namespace dstore
