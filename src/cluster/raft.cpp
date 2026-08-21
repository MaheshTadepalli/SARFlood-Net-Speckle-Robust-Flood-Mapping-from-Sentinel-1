#include "dstore/cluster/raft.h"

#include <algorithm>
#include <map>
#include <utility>

namespace dstore {

RaftNode::RaftNode(std::string node_id) : node_id_(std::move(node_id)) {}

VoteRequest RaftNode::start_election() {
  role_ = RaftRole::kCandidate;
  current_term_++;
  voted_for_ = node_id_;
  votes_received_ = 1;
  std::uint64_t last_index = log_.empty() ? 0 : log_.back().index;
  std::uint64_t last_term = log_.empty() ? 0 : log_.back().term;
  return {node_id_, current_term_, last_index, last_term};
}

VoteResponse RaftNode::request_vote(const VoteRequest& request) {
  if (request.term < current_term_) return {current_term_, false};
  if (request.term > current_term_) {
    current_term_ = request.term;
    role_ = RaftRole::kFollower;
    voted_for_.reset();
  }
  bool available = !voted_for_.has_value() || voted_for_.value() == request.candidate_id;
  bool grant = available && candidate_log_is_fresh(request);
  if (grant) voted_for_ = request.candidate_id;
  return {current_term_, grant};
}

AppendEntriesResponse RaftNode::append_entries(const AppendEntriesRequest& request) {
  if (request.term < current_term_) return {current_term_, false};
  if (request.term >= current_term_) {
    current_term_ = request.term;
    role_ = RaftRole::kFollower;
    voted_for_ = request.leader_id;
  }
  if (request.prev_log_index > 0) {
    if (log_.size() < request.prev_log_index) return {current_term_, false};
    if (log_[static_cast<std::size_t>(request.prev_log_index - 1)].term != request.prev_log_term) return {current_term_, false};
  }
  for (const auto& entry : request.entries) {
    if (log_.size() >= entry.index) log_.resize(static_cast<std::size_t>(entry.index - 1));
    log_.push_back(entry);
  }
  commit_index_ = std::min<std::uint64_t>(request.leader_commit, log_.empty() ? 0 : log_.back().index);
  return {current_term_, true};
}

Result<RaftLogEntry> RaftNode::append_client_command(std::string command) {
  if (role_ != RaftRole::kLeader) return Status::Unavailable("node is not raft leader");
  RaftLogEntry entry{log_.empty() ? 1 : log_.back().index + 1, current_term_, std::move(command)};
  log_.push_back(entry);
  commit_index_ = entry.index;
  return entry;
}

void RaftNode::observe_vote(const VoteResponse& response, std::size_t cluster_size) {
  if (response.term > current_term_) {
    current_term_ = response.term;
    role_ = RaftRole::kFollower;
    voted_for_.reset();
    return;
  }
  if (role_ == RaftRole::kCandidate && response.granted) votes_received_++;
  if (role_ == RaftRole::kCandidate && votes_received_ > cluster_size / 2) {
    role_ = RaftRole::kLeader;
    for (auto& [_, next] : next_index_) next = log_.empty() ? 1 : log_.back().index + 1;
    for (auto& [_, matched] : match_index_) matched = 0;
  }
}

AppendEntriesRequest RaftNode::make_append_entries(const std::string& follower_id) const {
  auto next_it = next_index_.find(follower_id);
  std::uint64_t next = next_it == next_index_.end() ? (log_.empty() ? 1 : log_.back().index + 1) : next_it->second;
  std::uint64_t prev_index = next == 0 ? 0 : next - 1;
  std::uint64_t prev_term = prev_index == 0 || log_.size() < prev_index ? 0 : log_[static_cast<std::size_t>(prev_index - 1)].term;
  std::vector<RaftLogEntry> entries;
  for (const auto& entry : log_) {
    if (entry.index >= next) entries.push_back(entry);
  }
  return {node_id_, current_term_, prev_index, prev_term, commit_index_, entries};
}

void RaftNode::observe_append_entries_response(const std::string& follower_id, const AppendEntriesResponse& response, std::size_t cluster_size) {
  if (response.term > current_term_) {
    current_term_ = response.term;
    role_ = RaftRole::kFollower;
    voted_for_.reset();
    return;
  }
  if (role_ != RaftRole::kLeader) return;
  if (!response.success) {
    auto& next = next_index_[follower_id];
    if (next > 1) next--;
    return;
  }
  auto last_index = log_.empty() ? 0 : log_.back().index;
  match_index_[follower_id] = last_index;
  next_index_[follower_id] = last_index + 1;
  for (std::uint64_t candidate = last_index; candidate > commit_index_; --candidate) {
    std::size_t replicated = 1;
    for (const auto& [_, matched] : match_index_) {
      if (matched >= candidate) replicated++;
    }
    if (replicated > cluster_size / 2 && log_[static_cast<std::size_t>(candidate - 1)].term == current_term_) {
      commit_index_ = candidate;
      break;
    }
  }
}

void RaftNode::configure_cluster(const std::vector<std::string>& peer_ids) {
  for (const auto& peer : peer_ids) {
    next_index_[peer] = log_.empty() ? 1 : log_.back().index + 1;
    match_index_[peer] = 0;
  }
}

void RaftNode::become_leader_for_single_node() {
  auto vote = start_election();
  (void)vote;
  observe_vote({current_term_, true}, 1);
}

bool RaftNode::candidate_log_is_fresh(const VoteRequest& request) const {
  std::uint64_t last_index = log_.empty() ? 0 : log_.back().index;
  std::uint64_t last_term = log_.empty() ? 0 : log_.back().term;
  return request.last_log_term > last_term || (request.last_log_term == last_term && request.last_log_index >= last_index);
}

}  // namespace dstore
