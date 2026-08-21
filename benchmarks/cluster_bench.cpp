#include <chrono>
#include <iostream>

#include "dstore/cluster/raft.h"

int main() {
  dstore::RaftNode node("bench");
  node.become_leader_for_single_node();
  auto raft_start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) node.append_client_command("entry-" + std::to_string(i));
  auto raft_end = std::chrono::steady_clock::now();

  std::cout << "raft_entries=100000\n";
  std::cout << "raft_elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(raft_end - raft_start).count() << "\n";
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(raft_end - raft_start).count();
  std::cout << "raft_append_avg_ns=" << (ns / 100000) << "\n";
}
