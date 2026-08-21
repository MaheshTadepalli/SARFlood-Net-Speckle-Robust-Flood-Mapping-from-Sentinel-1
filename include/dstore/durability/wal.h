#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "dstore/common/result.h"

namespace dstore {

enum class WalRecordType { kPutObject = 1, kDeleteObject = 2, kRaftEntry = 3, kHintedHandoff = 4 };

struct WalRecord {
  WalRecordType type = WalRecordType::kPutObject;
  std::string key;
  std::string payload;
};

class WriteAheadLog {
 public:
  explicit WriteAheadLog(std::filesystem::path path);
  Result<void> open();
  Result<void> append(const WalRecord& record);
  Result<std::vector<WalRecord>> replay() const;
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
  std::ofstream out_;
};

}  // namespace dstore
