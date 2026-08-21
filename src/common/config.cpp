#include "dstore/common/config.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace dstore {

static std::string trim(std::string s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') return s.substr(1, s.size() - 2);
  return s;
}

Result<Config> load_config(const std::filesystem::path& path) {
  Config cfg;
  std::ifstream in(path);
  if (!in) return Status::NotFound("config file not found: " + path.string());
  std::string line;
  while (std::getline(in, line)) {
    auto comment = line.find('#');
    if (comment != std::string::npos) line.resize(comment);
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    auto key = trim(line.substr(0, eq));
    auto value = trim(line.substr(eq + 1));
    if (key == "listen_host") cfg.listen_host = value;
    else if (key == "listen_port") cfg.listen_port = std::stoi(value);
    else if (key == "data_dir") cfg.data_dir = value;
    else if (key == "metadata_db") cfg.metadata_db = value;
    else if (key == "chunk_size") cfg.chunk_size = static_cast<std::size_t>(std::stoull(value));
    else if (key == "replication_factor") cfg.replication_factor = std::stoi(value);
    else if (key == "write_quorum") cfg.write_quorum = std::stoi(value);
    else if (key == "read_quorum") cfg.read_quorum = std::stoi(value);
    else if (key == "node_id") cfg.node_id = value;
    else if (key == "peer") cfg.peers.push_back(value);
    else if (key == "admin_token") cfg.admin_token = value;
    else if (key == "wal_path") cfg.wal_path = value;
    else if (key == "handoff_path") cfg.handoff_path = value;
  }
  return cfg;
}

}  // namespace dstore
