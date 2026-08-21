#include "dstore/cluster/distributed_chunk_service.h"

#include <algorithm>
#include <utility>

#include "dstore/common/logger.h"

namespace dstore {

DistributedChunkService::DistributedChunkService(ChunkStore& local, ClusterState& cluster, NodeClient client, StorageNode self,
                                                 int replication_factor, int write_quorum, int read_quorum, HintedHandoffQueue* handoff,
                                                 std::vector<StorageNode> seed_members)
    : local_(local),
      cluster_(cluster),
      client_(std::move(client)),
      self_(std::move(self)),
      seed_members_(std::move(seed_members)),
      replication_factor_(replication_factor),
      write_quorum_(write_quorum),
      read_quorum_(read_quorum),
      handoff_(handoff) {}

bool DistributedChunkService::is_self(const StorageNode& node) const { return node.id == self_.id; }

void DistributedChunkService::refresh_seed_members() {
  cluster_.heartbeat(self_);
  for (const auto& member : seed_members_) cluster_.heartbeat(member);
}

Result<ChunkRef> DistributedChunkService::put(const std::vector<std::uint8_t>& bytes) {
  refresh_seed_members();
  auto local_ref = local_.put(bytes);
  if (!local_ref.ok()) return local_ref.status();
  auto plan = cluster_.write_plan(local_ref.value().checksum, replication_factor_, write_quorum_);
  DSTORE_LOG_INFO("replication", "write plan for chunk " + local_ref.value().checksum + " has " + std::to_string(plan.targets.size()) + " targets");
  int acknowledgements = 1;
  for (const auto& node : plan.targets) {
    if (is_self(node)) {
      continue;
    }
    auto put = client_.put_chunk(node, local_ref.value(), bytes);
    if (put.ok()) acknowledgements++;
    else {
      DSTORE_LOG_WARN("replication", "failed to replicate chunk " + local_ref.value().checksum + " to " + node.id + ": " + put.status().message());
      if (handoff_) handoff_->enqueue(node, local_ref.value());
    }
  }
  DSTORE_LOG_INFO("replication", "chunk " + local_ref.value().checksum + " write acknowledgements=" + std::to_string(acknowledgements) + " quorum=" + std::to_string(write_quorum_));
  if (acknowledgements < write_quorum_) return Status::Unavailable("quorum write failed");
  return local_ref.value();
}

Result<std::vector<std::uint8_t>> DistributedChunkService::get(const std::string& checksum) {
  refresh_seed_members();
  auto plan = cluster_.read_plan(checksum, replication_factor_, read_quorum_);
  int responses = 0;
  Status last_error = Status::NotFound("chunk not found");
  std::vector<StorageNode> failed_replicas;
  std::vector<std::uint8_t> quorum_value;
  for (const auto& node : plan.targets) {
    Result<std::vector<std::uint8_t>> data = is_self(node) ? local_.get(checksum) : client_.get_chunk(node, checksum);
    if (data.ok()) {
      responses++;
      quorum_value = data.value();
      if (responses >= read_quorum_) {
        ChunkRef ref{checksum, static_cast<std::uint64_t>(quorum_value.size())};
        for (const auto& failed : failed_replicas) {
          if (is_self(failed)) local_.put(quorum_value);
          else client_.put_chunk(failed, ref, quorum_value);
        }
        return quorum_value;
      }
    } else {
      failed_replicas.push_back(node);
      last_error = data.status();
    }
  }
  auto local = local_.get(checksum);
  if (local.ok() && read_quorum_ <= 1) return local.value();
  return last_error.code() == Status::Code::kNotFound ? Status::Unavailable("quorum read failed") : last_error;
}

Result<void> DistributedChunkService::remove(const std::string& checksum, int references) {
  if (references > 0) return {};
  refresh_seed_members();
  auto plan = cluster_.write_plan(checksum, replication_factor_, write_quorum_);
  int acknowledgements = 0;
  for (const auto& node : plan.targets) {
    auto removed = is_self(node) ? local_.remove_if_unreferenced(checksum, 0) : client_.delete_chunk(node, checksum);
    if (removed.ok()) acknowledgements++;
  }
  if (acknowledgements < std::min(write_quorum_, static_cast<int>(plan.targets.size()))) return Status::Unavailable("quorum delete failed");
  return {};
}

Result<int> DistributedChunkService::replay_hinted_handoff() {
  if (!handoff_) return 0;
  auto loaded = handoff_->load();
  if (!loaded.ok()) return loaded.status();
  std::vector<HandoffHint> remaining;
  int delivered = 0;
  for (const auto& hint : loaded.value()) {
    auto bytes = local_.get(hint.checksum);
    if (!bytes.ok()) {
      remaining.push_back(hint);
      continue;
    }
    StorageNode target{hint.target_node_id, hint.target_address, true};
    auto sent = client_.put_chunk(target, {hint.checksum, hint.size}, bytes.value());
    if (sent.ok()) delivered++;
    else remaining.push_back(hint);
  }
  auto replaced = handoff_->replace(remaining);
  if (!replaced.ok()) return replaced.status();
  return delivered;
}

}  // namespace dstore
