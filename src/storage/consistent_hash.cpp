#include "dstore/storage/consistent_hash.h"

#include <algorithm>
#include <functional>
#include <set>

namespace dstore {

ConsistentHashRing::ConsistentHashRing(int virtual_nodes) : virtual_nodes_(virtual_nodes) {}

void ConsistentHashRing::add_node(const StorageNode& node) {
  for (int i = 0; i < virtual_nodes_; ++i) ring_[hash64(node.id + "#" + std::to_string(i))] = node;
}

void ConsistentHashRing::remove_node(const std::string& node_id) {
  for (auto it = ring_.begin(); it != ring_.end();) {
    if (it->second.id == node_id) it = ring_.erase(it);
    else ++it;
  }
}

std::vector<StorageNode> ConsistentHashRing::locate(const std::string& key, int replicas) const {
  std::vector<StorageNode> out;
  if (ring_.empty() || replicas <= 0) return out;
  std::set<std::string> seen;
  auto it = ring_.lower_bound(hash64(key));
  if (it == ring_.end()) it = ring_.begin();
  while (out.size() < static_cast<std::size_t>(replicas) && seen.size() < nodes().size()) {
    if (it->second.healthy && !seen.contains(it->second.id)) {
      out.push_back(it->second);
      seen.insert(it->second.id);
    }
    if (++it == ring_.end()) it = ring_.begin();
  }
  return out;
}

std::vector<StorageNode> ConsistentHashRing::nodes() const {
  std::vector<StorageNode> out;
  std::set<std::string> seen;
  for (const auto& [_, node] : ring_) {
    if (seen.insert(node.id).second) out.push_back(node);
  }
  return out;
}

std::uint64_t ConsistentHashRing::hash64(const std::string& value) {
  return static_cast<std::uint64_t>(std::hash<std::string>{}(value));
}

}  // namespace dstore
