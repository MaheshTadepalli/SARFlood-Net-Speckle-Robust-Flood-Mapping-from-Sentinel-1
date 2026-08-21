#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace dstore {

class ThreadPool {
 public:
  explicit ThreadPool(std::size_t workers);
  ~ThreadPool();
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  template <typename Fn>
  auto submit(Fn&& fn) -> std::future<decltype(fn())> {
    using Ret = decltype(fn());
    auto task = std::make_shared<std::packaged_task<Ret()>>(std::forward<Fn>(fn));
    auto fut = task->get_future();
    {
      std::lock_guard lock(mu_);
      jobs_.push([task] { (*task)(); });
    }
    cv_.notify_one();
    return fut;
  }

 private:
  void run();
  std::mutex mu_;
  std::condition_variable cv_;
  bool stop_{false};
  std::queue<std::function<void()>> jobs_;
  std::vector<std::thread> workers_;
};

}  // namespace dstore
