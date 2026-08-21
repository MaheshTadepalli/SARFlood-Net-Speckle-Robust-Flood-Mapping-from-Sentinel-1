#include "dstore/cluster/background_worker.h"

#include <utility>

#include "dstore/common/logger.h"

namespace dstore {

PeriodicWorker::PeriodicWorker(std::chrono::milliseconds interval, std::function<void()> task)
    : interval_(interval), task_(std::move(task)) {}

PeriodicWorker::~PeriodicWorker() { stop(); }

void PeriodicWorker::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) return;
  thread_ = std::thread([this] {
    while (running_.load()) {
      task_();
      std::this_thread::sleep_for(interval_);
    }
  });
}

void PeriodicWorker::stop() {
  running_.store(false);
  if (thread_.joinable()) thread_.join();
}

HeartbeatWorker::HeartbeatWorker(NodeClient client, std::string self_id, std::vector<StorageNode> peers, std::chrono::milliseconds interval,
                                 ClusterState* cluster)
    : client_(std::move(client)),
      self_id_(std::move(self_id)),
      peers_(std::move(peers)),
      cluster_(cluster),
      worker_(interval, [this] {
        for (const auto& peer : peers_) {
          auto result = client_.heartbeat(peer, self_id_);
          if (result.ok()) {
            if (cluster_) cluster_->heartbeat(peer);
          } else {
            DSTORE_LOG_WARN("heartbeat", "peer heartbeat failed: " + peer.id);
          }
        }
      }) {}

void HeartbeatWorker::start() { worker_.start(); }

void HeartbeatWorker::stop() { worker_.stop(); }

}  // namespace dstore
