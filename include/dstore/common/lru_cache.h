#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace dstore {

template <typename Key, typename Value>
class LruCache {
 public:
  explicit LruCache(std::size_t capacity) : capacity_(capacity) {}

  void put(Key key, Value value) {
    std::lock_guard lock(mu_);
    auto it = index_.find(key);
    if (it != index_.end()) {
      entries_.erase(it->second);
      index_.erase(it);
    }
    entries_.push_front({std::move(key), std::move(value)});
    index_[entries_.front().first] = entries_.begin();
    while (index_.size() > capacity_) {
      index_.erase(entries_.back().first);
      entries_.pop_back();
    }
  }

  std::optional<Value> get(const Key& key) {
    std::lock_guard lock(mu_);
    auto it = index_.find(key);
    if (it == index_.end()) return std::nullopt;
    entries_.splice(entries_.begin(), entries_, it->second);
    return it->second->second;
  }

  std::size_t size() const {
    std::lock_guard lock(mu_);
    return index_.size();
  }

 private:
  std::size_t capacity_;
  mutable std::mutex mu_;
  std::list<std::pair<Key, Value>> entries_;
  std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> index_;
};

}  // namespace dstore
