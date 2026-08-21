#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "dstore/common/result.h"

namespace dstore {

struct Config {
  std::string listen_host = "0.0.0.0";
  int listen_port = 8080;
  std::filesystem::path data_dir = "data";
  std::filesystem::path metadata_db = "data/metadata.sqlite";
  std::size_t chunk_size = 4 * 1024 * 1024;
  int replication_factor = 3;
  int write_quorum = 2;
  int read_quorum = 2;
  std::string node_id = "node-a";
  std::vector<std::string> peers;
  std::string admin_token = "admin-token";
  std::filesystem::path wal_path = "data/wal.log";
  std::filesystem::path handoff_path = "data/hinted-handoff.log";
};

Result<Config> load_config(const std::filesystem::path& path);

}  // namespace dstore
