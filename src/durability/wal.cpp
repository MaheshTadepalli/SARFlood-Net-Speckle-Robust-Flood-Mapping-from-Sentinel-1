#include "dstore/durability/wal.h"

#include <sstream>
#include <utility>

#include "dstore/common/sha256.h"

namespace dstore {

WriteAheadLog::WriteAheadLog(std::filesystem::path path) : path_(std::move(path)) {}

Result<void> WriteAheadLog::open() {
  std::filesystem::create_directories(path_.parent_path());
  out_.open(path_, std::ios::binary | std::ios::app);
  if (!out_) return Status::Internal("failed to open wal");
  return {};
}

Result<void> WriteAheadLog::append(const WalRecord& record) {
  if (!out_) return Status::Internal("wal is not open");
  std::string line = std::to_string(static_cast<int>(record.type)) + "\t" + record.key + "\t" + record.payload;
  std::string checksum = Sha256::hex(line);
  out_ << line << "\t" << checksum << "\n";
  out_.flush();
  if (!out_) return Status::Internal("failed to append wal record");
  return {};
}

Result<std::vector<WalRecord>> WriteAheadLog::replay() const {
  std::ifstream in(path_, std::ios::binary);
  if (!in) return std::vector<WalRecord>{};
  std::vector<WalRecord> records;
  std::string line;
  while (std::getline(in, line)) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
      auto tab = line.find('\t', start);
      if (tab == std::string::npos) {
        parts.push_back(line.substr(start));
        break;
      }
      parts.push_back(line.substr(start, tab - start));
      start = tab + 1;
    }
    if (parts.size() != 4) return Status::Internal("corrupt wal record");
    std::string payload = parts[0] + "\t" + parts[1] + "\t" + parts[2];
    if (Sha256::hex(payload) != parts[3]) return Status::Internal("wal checksum mismatch");
    records.push_back({static_cast<WalRecordType>(std::stoi(parts[0])), parts[1], parts[2]});
  }
  return records;
}

}  // namespace dstore
