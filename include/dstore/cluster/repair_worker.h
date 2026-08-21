#pragma once

#include "dstore/cluster/distributed_chunk_service.h"
#include "dstore/metadata/metadata_store.h"

namespace dstore {

class RepairWorker {
 public:
  RepairWorker(MetadataStore& metadata, DistributedChunkService& chunks);
  Result<int> repair_bucket(const std::string& bucket);

 private:
  MetadataStore& metadata_;
  DistributedChunkService& chunks_;
};

class RebalanceWorker {
 public:
  RebalanceWorker(MetadataStore& metadata, DistributedChunkService& chunks);
  Result<int> rebalance_bucket(const std::string& bucket);

 private:
  MetadataStore& metadata_;
  DistributedChunkService& chunks_;
};

}  // namespace dstore
