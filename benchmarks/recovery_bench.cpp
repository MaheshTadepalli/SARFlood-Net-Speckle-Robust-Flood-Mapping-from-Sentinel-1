#include <chrono>
#include <filesystem>
#include <iostream>

#include "dstore/durability/wal.h"

int main() {
  auto path = std::filesystem::temp_directory_path() / "dstore_recovery_bench.log";
  std::filesystem::remove(path);
  {
    dstore::WriteAheadLog wal(path);
    auto opened = wal.open();
    if (!opened.ok()) return 1;
    for (int i = 0; i < 100000; ++i) {
      auto appended = wal.append({dstore::WalRecordType::kPutObject, "bucket/object-" + std::to_string(i), "checksum-" + std::to_string(i)});
      if (!appended.ok()) return 2;
    }
  }
  dstore::WriteAheadLog wal(path);
  auto start = std::chrono::steady_clock::now();
  auto records = wal.replay();
  auto end = std::chrono::steady_clock::now();
  if (!records.ok()) return 3;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
  std::cout << "wal_records=" << records.value().size() << "\n";
  std::cout << "recovery_elapsed_ms=" << ms << "\n";
  std::filesystem::remove(path);
}
