#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace dstore {

struct StorageNode {
  std::string id;
  std::string address;
  bool healthy = true;
};

class ConsistentHashRing {
 public:
  explicit ConsistentHashRing(int virtual_nodes = 128);
  void add_node(const StorageNode& node);
  void remove_node(const std::string& node_id);
  std::vector<StorageNode> locate(const std::string& key, int replicas) const;
  std::vector<StorageNode> nodes() const;

 private:
  static std::uint64_t hash64(const std::string& value);
  int virtual_nodes_;
  std::map<std::uint64_t, StorageNode> ring_;
};

}  // namespace dstore
