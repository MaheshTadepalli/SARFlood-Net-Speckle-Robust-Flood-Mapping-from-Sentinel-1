#pragma once

#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <optional>
#include <string>
#include <vector>

#include "dstore/common/result.h"

namespace dstore {

enum class RaftRole { kFollower, kCandidate, kLeader };

struct RaftLogEntry {
  std::uint64_t index = 0;
  std::uint64_t term = 0;
  std::string command;
};

struct VoteRequest {
  std::string candidate_id;
  std::uint64_t term = 0;
  std::uint64_t last_log_index = 0;
  std::uint64_t last_log_term = 0;
};

struct VoteResponse {
  std::uint64_t term = 0;
  bool granted = false;
};

struct AppendEntriesRequest {
  std::string leader_id;
  std::uint64_t term = 0;
  std::uint64_t prev_log_index = 0;
  std::uint64_t prev_log_term = 0;
  std::uint64_t leader_commit = 0;
  std::vector<RaftLogEntry> entries;
};

struct AppendEntriesResponse {
  std::uint64_t term = 0;
  bool success = false;
};

class RaftNode {
 public:
  explicit RaftNode(std::string node_id);
  VoteRequest start_election();
  VoteResponse request_vote(const VoteRequest& request);
  AppendEntriesResponse append_entries(const AppendEntriesRequest& request);
  Result<RaftLogEntry> append_client_command(std::string command);
  void observe_vote(const VoteResponse& response, std::size_t cluster_size);
  AppendEntriesRequest make_append_entries(const std::string& follower_id) const;
  void observe_append_entries_response(const std::string& follower_id, const AppendEntriesResponse& response, std::size_t cluster_size);
  void configure_cluster(const std::vector<std::string>& peer_ids);
  void become_leader_for_single_node();

  const std::string& id() const { return node_id_; }
  RaftRole role() const { return role_; }
  std::uint64_t term() const { return current_term_; }
  std::uint64_t commit_index() const { return commit_index_; }
  const std::vector<RaftLogEntry>& log() const { return log_; }
  std::optional<std::string> voted_for() const { return voted_for_; }

 private:
  bool candidate_log_is_fresh(const VoteRequest& request) const;

  std::string node_id_;
  RaftRole role_ = RaftRole::kFollower;
  std::uint64_t current_term_ = 0;
  std::optional<std::string> voted_for_;
  std::vector<RaftLogEntry> log_;
  std::uint64_t commit_index_ = 0;
  std::size_t votes_received_ = 0;
  std::unordered_map<std::string, std::uint64_t> next_index_;
  std::unordered_map<std::string, std::uint64_t> match_index_;
};

}  // namespace dstore
