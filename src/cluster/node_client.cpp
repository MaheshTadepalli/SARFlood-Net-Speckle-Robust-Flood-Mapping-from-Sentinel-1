#include "dstore/cluster/node_client.h"

#include <utility>

#include "dstore/common/logger.h"
#include "dstore/network/http_server.h"

namespace dstore {
namespace {
std::pair<std::string, int> split_address(const std::string& address) {
  auto colon = address.rfind(':');
  if (colon == std::string::npos) return {address, 8080};
  return {address.substr(0, colon), std::stoi(address.substr(colon + 1))};
}
}  // namespace

NodeClient::NodeClient(std::string bearer_token) : token_(std::move(bearer_token)) {}

Result<void> NodeClient::put_chunk(const StorageNode& node, const ChunkRef& ref, const std::vector<std::uint8_t>& bytes) const {
  auto [host, port] = split_address(node.address);
  HttpRequest req;
  req.method = "PUT";
  req.path = "/v1/chunks/" + ref.checksum;
  req.headers["Authorization"] = "Bearer " + token_;
  req.headers["X-Chunk-Size"] = std::to_string(ref.size);
  req.body = bytes;
  auto resp = http_request(host, port, req);
  if (!resp.ok()) return resp.status();
  if (resp.value().status >= 300) {
    DSTORE_LOG_WARN("node_client", "chunk put to " + node.id + " returned HTTP " + std::to_string(resp.value().status));
    return Status::Unavailable("remote chunk put failed on " + node.id);
  }
  return {};
}

Result<std::vector<std::uint8_t>> NodeClient::get_chunk(const StorageNode& node, const std::string& checksum) const {
  auto [host, port] = split_address(node.address);
  HttpRequest req;
  req.method = "GET";
  req.path = "/v1/chunks/" + checksum;
  req.headers["Authorization"] = "Bearer " + token_;
  auto resp = http_request(host, port, req);
  if (!resp.ok()) return resp.status();
  if (resp.value().status >= 300) return Status::NotFound("remote chunk not found on " + node.id);
  return resp.value().body;
}

Result<void> NodeClient::delete_chunk(const StorageNode& node, const std::string& checksum) const {
  auto [host, port] = split_address(node.address);
  HttpRequest req;
  req.method = "DELETE";
  req.path = "/v1/chunks/" + checksum;
  req.headers["Authorization"] = "Bearer " + token_;
  auto resp = http_request(host, port, req);
  if (!resp.ok()) return resp.status();
  if (resp.value().status >= 300) return Status::Unavailable("remote chunk delete failed on " + node.id);
  return {};
}

Result<void> NodeClient::heartbeat(const StorageNode& node, const std::string& from_node_id) const {
  auto [host, port] = split_address(node.address);
  HttpRequest req;
  req.method = "POST";
  req.path = "/v1/heartbeat";
  req.headers["Authorization"] = "Bearer " + token_;
  req.body.assign(from_node_id.begin(), from_node_id.end());
  auto resp = http_request(host, port, req);
  if (!resp.ok()) return resp.status();
  if (resp.value().status >= 300) {
    DSTORE_LOG_WARN("node_client", "heartbeat to " + node.id + " returned HTTP " + std::to_string(resp.value().status));
    return Status::Unavailable("heartbeat failed on " + node.id);
  }
  return {};
}

StorageNode parse_node(std::string spec) {
  auto eq = spec.find('=');
  if (eq == std::string::npos) return {spec, spec, true};
  return {spec.substr(0, eq), spec.substr(eq + 1), true};
}

}  // namespace dstore
