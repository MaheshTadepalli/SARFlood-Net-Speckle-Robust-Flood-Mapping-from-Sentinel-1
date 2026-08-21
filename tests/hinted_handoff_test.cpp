#include <gtest/gtest.h>

#include <filesystem>

#include "dstore/cluster/hinted_handoff.h"

TEST(HintedHandoffTest, PersistsAndRewritesHints) {
  auto path = std::filesystem::temp_directory_path() / "dstore_handoff_test.log";
  std::filesystem::remove(path);
  dstore::HintedHandoffQueue queue(path);
  ASSERT_TRUE(queue.enqueue({"node-b", "node-b:8080", true}, {"abc", 3}).ok());
  auto loaded = queue.load();
  ASSERT_TRUE(loaded.ok());
  ASSERT_EQ(loaded.value().size(), 1u);
  EXPECT_EQ(loaded.value()[0].target_node_id, "node-b");
  EXPECT_EQ(loaded.value()[0].checksum, "abc");
  ASSERT_TRUE(queue.replace({}).ok());
  auto empty = queue.load();
  ASSERT_TRUE(empty.ok());
  EXPECT_TRUE(empty.value().empty());
  std::filesystem::remove(path);
}
