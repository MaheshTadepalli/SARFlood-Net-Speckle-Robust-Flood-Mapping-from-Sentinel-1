#include <gtest/gtest.h>

#include "dstore/cluster/raft.h"

TEST(RaftTest, CandidateWinsMajorityAndAppendsAsLeader) {
  dstore::RaftNode a("a");
  dstore::RaftNode b("b");
  dstore::RaftNode c("c");
  auto req = a.start_election();
  a.observe_vote(b.request_vote(req), 3);
  a.observe_vote(c.request_vote(req), 3);
  ASSERT_EQ(a.role(), dstore::RaftRole::kLeader);
  auto entry = a.append_client_command("put object");
  ASSERT_TRUE(entry.ok());
  EXPECT_EQ(entry.value().index, 1u);
}

TEST(RaftTest, AppendEntriesRejectsConflictingTerm) {
  dstore::RaftNode follower("f");
  auto res = follower.append_entries({"leader", 2, 1, 99, 0, {}});
  EXPECT_FALSE(res.success);
}

TEST(RaftTest, LeaderAdvancesCommitAfterFollowerAck) {
  dstore::RaftNode leader("a");
  leader.configure_cluster({"b", "c"});
  auto vote = leader.start_election();
  leader.observe_vote({vote.term, true}, 3);
  ASSERT_EQ(leader.role(), dstore::RaftRole::kLeader);
  ASSERT_TRUE(leader.append_client_command("put").ok());
  auto append = leader.make_append_entries("b");
  ASSERT_EQ(append.entries.size(), 1u);
  leader.observe_append_entries_response("b", {leader.term(), true}, 3);
  EXPECT_EQ(leader.commit_index(), 1u);
}
