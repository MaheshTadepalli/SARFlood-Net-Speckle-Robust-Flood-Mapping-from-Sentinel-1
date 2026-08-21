#include "dstore/cluster/hinted_handoff.h"

#include <fstream>
#include <sstream>
#include <utility>

namespace dstore {

HintedHandoffQueue::HintedHandoffQueue(std::filesystem::path path) : path_(std::move(path)) {}

Result<void> HintedHandoffQueue::enqueue(const StorageNode& target, const ChunkRef& chunk) {
  std::lock_guard lock(mu_);
  std::filesystem::create_directories(path_.parent_path());
  std::ofstream out(path_, std::ios::app | std::ios::binary);
  if (!out) return Status::Internal("failed to open hinted handoff queue");
  out << target.id << "\t" << target.address << "\t" << chunk.checksum << "\t" << chunk.size << "\n";
  if (!out) return Status::Internal("failed to append hinted handoff queue");
  return {};
}

Result<std::vector<HandoffHint>> HintedHandoffQueue::load() const {
  std::lock_guard lock(mu_);
  std::ifstream in(path_, std::ios::binary);
  if (!in) return std::vector<HandoffHint>{};
  std::vector<HandoffHint> hints;
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream row(line);
    HandoffHint hint;
    row >> hint.target_node_id >> hint.target_address >> hint.checksum >> hint.size;
    if (!hint.target_node_id.empty() && !hint.checksum.empty()) hints.push_back(hint);
  }
  return hints;
}

Result<void> HintedHandoffQueue::replace(const std::vector<HandoffHint>& hints) {
  std::lock_guard lock(mu_);
  std::filesystem::create_directories(path_.parent_path());
  std::ofstream out(path_, std::ios::trunc | std::ios::binary);
  if (!out) return Status::Internal("failed to rewrite hinted handoff queue");
  for (const auto& hint : hints) {
    out << hint.target_node_id << "\t" << hint.target_address << "\t" << hint.checksum << "\t" << hint.size << "\n";
  }
  return {};
}

}  // namespace dstore
