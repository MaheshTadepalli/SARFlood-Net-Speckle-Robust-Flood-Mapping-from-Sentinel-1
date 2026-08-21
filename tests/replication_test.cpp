#include <gtest/gtest.h>

#include "dstore/storage/replication.h"

TEST(ReplicationTest, ElectsStableLowestHealthyNode) {
  dstore::ClusterState cluster;
  cluster.heartbeat({"node-b", "b:1", true});
  cluster.heartbeat({"node-a", "a:1", true});
  auto leader = cluster.elect_leader(std::chrono::seconds(30));
  ASSERT_TRUE(leader.ok());
  EXPECT_EQ(leader.value().id, "node-a");
}

TEST(ReplicationTest, PlansQuorumTargets) {
  dstore::ClusterState cluster;
  cluster.heartbeat({"a", "a:1", true});
  cluster.heartbeat({"b", "b:1", true});
  cluster.heartbeat({"c", "c:1", true});
  auto plan = cluster.write_plan("chunk", 3, 2);
  EXPECT_EQ(plan.quorum, 2);
  EXPECT_EQ(plan.targets.size(), 3u);
}

TEST(ReplicationTest, WritePlanUsesKnownMembersEvenWithoutFreshTimeoutChecks) {
  dstore::ClusterState cluster;
  cluster.heartbeat({"a", "a:1", true});
  cluster.heartbeat({"b", "b:1", true});
  auto plan = cluster.write_plan("chunk", 2, 2);
  EXPECT_EQ(plan.targets.size(), 2u);
}
