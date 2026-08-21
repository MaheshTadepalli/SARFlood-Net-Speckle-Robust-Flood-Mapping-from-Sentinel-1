#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "dstore/cluster/node_client.h"
#include "dstore/storage/replication.h"

namespace dstore {

class PeriodicWorker {
 public:
  PeriodicWorker(std::chrono::milliseconds interval, std::function<void()> task);
  ~PeriodicWorker();
  PeriodicWorker(const PeriodicWorker&) = delete;
  PeriodicWorker& operator=(const PeriodicWorker&) = delete;
  void start();
  void stop();

 private:
  std::chrono::milliseconds interval_;
  std::function<void()> task_;
  std::atomic<bool> running_{false};
  std::thread thread_;
};

class HeartbeatWorker {
 public:
  HeartbeatWorker(NodeClient client, std::string self_id, std::vector<StorageNode> peers, std::chrono::milliseconds interval,
                  ClusterState* cluster = nullptr);
  void start();
  void stop();

 private:
  NodeClient client_;
  std::string self_id_;
  std::vector<StorageNode> peers_;
  ClusterState* cluster_;
  PeriodicWorker worker_;
};

}  // namespace dstore
