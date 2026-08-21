#include <gtest/gtest.h>

#include <filesystem>

#include "dstore/durability/wal.h"

TEST(WalTest, ReplaysChecksummedRecords) {
  auto path = std::filesystem::temp_directory_path() / "dstore_wal_test.log";
  std::filesystem::remove(path);
  {
    dstore::WriteAheadLog wal(path);
    ASSERT_TRUE(wal.open().ok());
    ASSERT_TRUE(wal.append({dstore::WalRecordType::kPutObject, "b/k", "checksum"}).ok());
  }
  dstore::WriteAheadLog wal(path);
  auto replay = wal.replay();
  ASSERT_TRUE(replay.ok());
  ASSERT_EQ(replay.value().size(), 1u);
  EXPECT_EQ(replay.value()[0].key, "b/k");
  std::filesystem::remove(path);
}
