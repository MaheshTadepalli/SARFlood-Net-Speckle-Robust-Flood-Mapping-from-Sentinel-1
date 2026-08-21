#include <gtest/gtest.h>

#include <filesystem>

#include "dstore/storage/chunk_store.h"

TEST(ChunkStoreTest, DeduplicatesAndReadsBackVerifiedChunks) {
  auto root = std::filesystem::temp_directory_path() / "dstore_chunk_test";
  std::filesystem::remove_all(root);
  dstore::ChunkStore store(root, 1024 * 1024);
  std::vector<std::uint8_t> data{'h', 'e', 'l', 'l', 'o'};
  auto first = store.put(data);
  auto second = store.put(data);
  ASSERT_TRUE(first.ok());
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(first.value().checksum, second.value().checksum);
  auto read = store.get(first.value().checksum);
  ASSERT_TRUE(read.ok());
  EXPECT_EQ(read.value(), data);
  std::filesystem::remove_all(root);
}
