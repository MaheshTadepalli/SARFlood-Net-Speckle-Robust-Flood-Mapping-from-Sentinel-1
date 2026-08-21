#pragma once

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "dstore/common/result.h"
#include "dstore/storage/consistent_hash.h"

namespace dstore {

struct ReplicaPlan {
  std::vector<StorageNode> targets;
  int quorum = 0;
};

class ClusterState {
 public:
  void heartbeat(StorageNode node);
  std::vector<StorageNode> known_nodes() const;
  Result<StorageNode> elect_leader(std::chrono::seconds timeout) const;
  std::vector<StorageNode> healthy_nodes(std::chrono::seconds timeout) const;
  ReplicaPlan write_plan(const std::string& chunk_id, int replicas, int quorum) const;
  ReplicaPlan read_plan(const std::string& chunk_id, int replicas, int quorum) const;
  std::vector<StorageNode> migration_targets(const std::string& chunk_id, int replicas) const;

 private:
  struct NodeState {
    StorageNode node;
    std::chrono::steady_clock::time_point last_seen;
  };
  mutable std::mutex mu_;
  std::unordered_map<std::string, NodeState> nodes_;
};

}  // namespace dstore
