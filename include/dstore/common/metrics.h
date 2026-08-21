#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace dstore {

class Metrics {
 public:
  void record_upload(std::uint64_t bytes);
  void record_download(std::uint64_t bytes);
  void record_delete();
  void record_error();
  std::string prometheus() const;

 private:
  std::atomic<std::uint64_t> uploads_{0};
  std::atomic<std::uint64_t> downloads_{0};
  std::atomic<std::uint64_t> deletes_{0};
  std::atomic<std::uint64_t> errors_{0};
  std::atomic<std::uint64_t> bytes_in_{0};
  std::atomic<std::uint64_t> bytes_out_{0};
};

}  // namespace dstore
