#include <gtest/gtest.h>

#include "dstore/storage/consistent_hash.h"

TEST(ConsistentHashTest, ReturnsDistinctHealthyReplicas) {
  dstore::ConsistentHashRing ring(32);
  ring.add_node({"a", "a:1", true});
  ring.add_node({"b", "b:1", true});
  ring.add_node({"c", "c:1", true});
  auto nodes = ring.locate("chunk-1", 3);
  ASSERT_EQ(nodes.size(), 3u);
  EXPECT_NE(nodes[0].id, nodes[1].id);
  EXPECT_NE(nodes[1].id, nodes[2].id);
}
