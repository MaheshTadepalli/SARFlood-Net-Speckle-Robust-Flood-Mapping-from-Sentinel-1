#include <gtest/gtest.h>

#include "dstore/storage/replication.h"

TEST(ReadRepairTest, RepairPlanUsesCurrentHealthyPlacement) {
  dstore::ClusterState cluster;
  cluster.heartbeat({"node-a", "a:8080", true});
  cluster.heartbeat({"node-b", "b:8080", true});
  cluster.heartbeat({"node-c", "c:8080", true});
  auto plan = cluster.read_plan("chunk-x", 3, 2);
  ASSERT_EQ(plan.quorum, 2);
  EXPECT_EQ(plan.targets.size(), 3u);
}
