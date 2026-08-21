#include "dstore/cluster/repair_worker.h"

namespace dstore {

RepairWorker::RepairWorker(MetadataStore& metadata, DistributedChunkService& chunks) : metadata_(metadata), chunks_(chunks) {}

Result<int> RepairWorker::repair_bucket(const std::string& bucket) {
  auto objects = metadata_.list_objects(bucket);
  if (!objects.ok()) return objects.status();
  int repaired = 0;
  for (const auto& object : objects.value()) {
    auto full = metadata_.get_object(object.bucket, object.key);
    if (!full.ok()) continue;
    for (const auto& chunk : full.value().chunks) {
      auto bytes = chunks_.get(chunk.checksum);
      if (bytes.ok()) {
        auto put = chunks_.put(bytes.value());
        if (put.ok()) repaired++;
      }
    }
  }
  return repaired;
}

RebalanceWorker::RebalanceWorker(MetadataStore& metadata, DistributedChunkService& chunks) : metadata_(metadata), chunks_(chunks) {}

Result<int> RebalanceWorker::rebalance_bucket(const std::string& bucket) {
  RepairWorker repair(metadata_, chunks_);
  return repair.repair_bucket(bucket);
}

}  // namespace dstore
