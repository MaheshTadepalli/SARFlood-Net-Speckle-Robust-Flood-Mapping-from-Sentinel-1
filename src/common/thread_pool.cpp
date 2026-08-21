#include "dstore/common/thread_pool.h"

namespace dstore {

ThreadPool::ThreadPool(std::size_t workers) {
  workers = workers == 0 ? 1 : workers;
  for (std::size_t i = 0; i < workers; ++i) workers_.emplace_back([this] { run(); });
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard lock(mu_);
    stop_ = true;
  }
  cv_.notify_all();
  for (auto& worker : workers_) {
    if (worker.joinable()) worker.join();
  }
}

void ThreadPool::run() {
  while (true) {
    std::function<void()> job;
    {
      std::unique_lock lock(mu_);
      cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
      if (stop_ && jobs_.empty()) return;
      job = std::move(jobs_.front());
      jobs_.pop();
    }
    job();
  }
}

}  // namespace dstore
