#include "dstore/storage/replication.h"

#include <algorithm>
#include <mutex>

namespace dstore {

void ClusterState::heartbeat(StorageNode node) {
  std::lock_guard lock(mu_);
  nodes_[node.id] = NodeState{std::move(node), std::chrono::steady_clock::now()};
}

std::vector<StorageNode> ClusterState::known_nodes() const {
  std::lock_guard lock(mu_);
  std::vector<StorageNode> out;
  for (const auto& [_, state] : nodes_) out.push_back(state.node);
  return out;
}

std::vector<StorageNode> ClusterState::healthy_nodes(std::chrono::seconds timeout) const {
  std::lock_guard lock(mu_);
  std::vector<StorageNode> out;
  auto now = std::chrono::steady_clock::now();
  for (const auto& [_, state] : nodes_) {
    auto node = state.node;
    node.healthy = now - state.last_seen <= timeout;
    if (node.healthy) out.push_back(node);
  }
  return out;
}

Result<StorageNode> ClusterState::elect_leader(std::chrono::seconds timeout) const {
  auto nodes = healthy_nodes(timeout);
  if (nodes.empty()) return Status::Unavailable("no healthy nodes available for leader election");
  std::sort(nodes.begin(), nodes.end(), [](const StorageNode& a, const StorageNode& b) { return a.id < b.id; });
  return nodes.front();
}

ReplicaPlan ClusterState::write_plan(const std::string& chunk_id, int replicas, int quorum) const {
  ConsistentHashRing ring;
  for (const auto& node : known_nodes()) ring.add_node(node);
  return {ring.locate(chunk_id, replicas), quorum};
}

ReplicaPlan ClusterState::read_plan(const std::string& chunk_id, int replicas, int quorum) const {
  return write_plan(chunk_id, replicas, quorum);
}

std::vector<StorageNode> ClusterState::migration_targets(const std::string& chunk_id, int replicas) const {
  return write_plan(chunk_id, replicas, replicas).targets;
}

}  // namespace dstore
