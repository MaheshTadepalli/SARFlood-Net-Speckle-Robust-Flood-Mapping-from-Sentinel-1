#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_map>

#include "dstore/common/auth.h"
#include "dstore/common/config.h"
#include "dstore/common/logger.h"
#include "dstore/common/metrics.h"
#include "dstore/common/sha256.h"
#include "dstore/cluster/background_worker.h"
#include "dstore/cluster/distributed_chunk_service.h"
#include "dstore/cluster/hinted_handoff.h"
#include "dstore/cluster/node_client.h"
#include "dstore/cluster/repair_worker.h"
#include "dstore/durability/wal.h"
#include "dstore/metadata/metadata_store.h"
#include "dstore/network/http_server.h"
#include "dstore/storage/chunk_store.h"
#include "dstore/storage/replication.h"

using namespace dstore;

namespace {
std::vector<std::uint8_t> bytes(const std::string& s) { return {s.begin(), s.end()}; }
std::string str(const std::vector<std::uint8_t>& b) { return {b.begin(), b.end()}; }

std::string bearer(const HttpRequest& req) {
  auto it = req.headers.find("authorization");
  if (it == req.headers.end()) return {};
  const std::string prefix = "Bearer ";
  if (it->second.rfind(prefix, 0) == 0) return it->second.substr(prefix.size());
  return {};
}

HttpResponse status_response(const Status& status) {
  int code = 500;
  if (status.code() == Status::Code::kInvalidArgument) code = 400;
  if (status.code() == Status::Code::kUnauthenticated) code = 401;
  if (status.code() == Status::Code::kPermissionDenied) code = 403;
  if (status.code() == Status::Code::kNotFound) code = 404;
  if (status.code() == Status::Code::kConflict) code = 409;
  std::string body = "{\"error\":\"" + status.message() + "\"}\n";
  return {code, "application/json", bytes(body)};
}

std::pair<std::string, std::string> object_path(const std::string& path) {
  const std::string p = "/v1/objects/";
  auto rest = path.substr(p.size());
  auto slash = rest.find('/');
  if (slash == std::string::npos) return {"", ""};
  return {rest.substr(0, slash), rest.substr(slash + 1)};
}
}  // namespace

int main(int argc, char** argv) {
  std::filesystem::path cfg_path = "config/dev.toml";
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--config") cfg_path = argv[i + 1];
  }
  auto cfg_result = load_config(cfg_path);
  if (!cfg_result.ok()) {
    DSTORE_LOG_ERROR("server", cfg_result.status().message());
    return 1;
  }
  auto cfg = cfg_result.value();
  MetadataStore metadata(cfg.metadata_db);
  auto opened = metadata.open();
  if (!opened.ok()) {
    DSTORE_LOG_ERROR("metadata", opened.status().message());
    return 1;
  }
  ChunkStore local_chunks(cfg.data_dir / "chunks", 64 * 1024 * 1024);
  AuthManager auth;
  auth.add_token(cfg.admin_token, {Permission::kAdmin});
  Metrics metrics;
  ClusterState cluster;
  StorageNode self{cfg.node_id, cfg.node_id + ":" + std::to_string(cfg.listen_port), true};
  cluster.heartbeat(self);
  std::vector<StorageNode> peers;
  std::unordered_map<std::string, std::string> peer_addresses;
  for (const auto& peer : cfg.peers) {
    peers.push_back(parse_node(peer));
    peer_addresses[peers.back().id] = peers.back().address;
    cluster.heartbeat(peers.back());
  }
  NodeClient node_client(cfg.admin_token);
  HintedHandoffQueue handoff(cfg.handoff_path);
  DSTORE_LOG_INFO("cluster", "configured peers: " + std::to_string(peers.size()));
  DistributedChunkService chunks(local_chunks, cluster, node_client, self, cfg.replication_factor, cfg.write_quorum, cfg.read_quorum, &handoff, peers);
  RepairWorker repair_worker(metadata, chunks);
  RebalanceWorker rebalance_worker(metadata, chunks);
  HeartbeatWorker heartbeat_worker(node_client, cfg.node_id, peers, std::chrono::seconds(5), &cluster);
  WriteAheadLog wal(cfg.wal_path);
  auto wal_open = wal.open();
  if (!wal_open.ok()) {
    DSTORE_LOG_ERROR("wal", wal_open.status().message());
    return 1;
  }
  auto replayed = wal.replay();
  if (!replayed.ok()) {
    DSTORE_LOG_ERROR("wal", replayed.status().message());
    return 1;
  }
  DSTORE_LOG_INFO("wal", "validated wal records: " + std::to_string(replayed.value().size()));

  HttpServer server(cfg.listen_host, cfg.listen_port, std::thread::hardware_concurrency());

  server.add_route("PUT", "/v1/objects/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kWrite);
    if (!az.ok()) return status_response(az.status());
    auto [bucket, key] = object_path(req.path);
    if (bucket.empty() || key.empty()) return status_response(Status::Invalid("expected /v1/objects/{bucket}/{key}"));
    auto logged = wal.append({WalRecordType::kPutObject, bucket + "/" + key, Sha256::hex(req.body)});
    if (!logged.ok()) return status_response(logged.status());
    ObjectMetadata meta{bucket, key, static_cast<std::uint64_t>(req.body.size()), Sha256::hex(req.body), {}};
    for (const auto& part : chunk_bytes(req.body, cfg.chunk_size)) {
      auto ref = chunks.put(part);
      if (!ref.ok()) return status_response(ref.status());
      meta.chunks.push_back(ref.value());
    }
    auto saved = metadata.put_object(meta);
    if (!saved.ok()) return status_response(saved.status());
    metrics.record_upload(req.body.size());
    std::ostringstream body;
    body << "{\"bucket\":\"" << bucket << "\",\"key\":\"" << key << "\",\"checksum\":\"" << meta.checksum
         << "\",\"chunks\":" << meta.chunks.size() << "}\n";
    return HttpResponse{201, "application/json", bytes(body.str())};
  });

  server.add_route("GET", "/v1/objects/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kRead);
    if (!az.ok()) return status_response(az.status());
    auto [bucket, key] = object_path(req.path);
    auto meta = metadata.get_object(bucket, key);
    if (!meta.ok()) return status_response(meta.status());
    std::vector<std::uint8_t> out;
    for (const auto& ref : meta.value().chunks) {
      auto plan = cluster.read_plan(ref.checksum, cfg.replication_factor, cfg.read_quorum);
      if (static_cast<int>(plan.targets.size()) < std::min(cfg.read_quorum, cfg.replication_factor)) {
        return status_response(Status::Unavailable("insufficient healthy replicas for quorum read"));
      }
      auto data = chunks.get(ref.checksum);
      if (!data.ok()) return status_response(data.status());
      out.insert(out.end(), data.value().begin(), data.value().end());
    }
    if (Sha256::hex(out) != meta.value().checksum) return status_response(Status::Internal("object checksum mismatch"));
    metrics.record_download(out.size());
    return HttpResponse{200, "application/octet-stream", std::move(out)};
  });

  server.add_route("DELETE", "/v1/objects/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kDelete);
    if (!az.ok()) return status_response(az.status());
    auto [bucket, key] = object_path(req.path);
    auto meta = metadata.get_object(bucket, key);
    if (!meta.ok()) return status_response(meta.status());
    auto logged = wal.append({WalRecordType::kDeleteObject, bucket + "/" + key, "delete"});
    if (!logged.ok()) return status_response(logged.status());
    auto deleted = metadata.delete_object(bucket, key);
    if (!deleted.ok()) return status_response(deleted.status());
    for (const auto& ref : meta.value().chunks) {
      auto refs = metadata.chunk_ref_count(ref.checksum);
      if (refs.ok()) chunks.remove(ref.checksum, refs.value());
    }
    metrics.record_delete();
    return HttpResponse{204, "application/json", {}};
  });

  server.add_route("GET", "/v1/metadata/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kRead);
    if (!az.ok()) return status_response(az.status());
    auto [bucket, key] = object_path("/v1/objects/" + req.path.substr(std::string("/v1/metadata/").size()));
    auto meta = metadata.get_object(bucket, key);
    if (!meta.ok()) return status_response(meta.status());
    std::ostringstream body;
    body << "{\"bucket\":\"" << meta.value().bucket << "\",\"key\":\"" << meta.value().key << "\",\"size\":"
         << meta.value().size << ",\"checksum\":\"" << meta.value().checksum << "\",\"chunks\":" << meta.value().chunks.size() << "}\n";
    return HttpResponse{200, "application/json", bytes(body.str())};
  });

  server.add_route("POST", "/v1/heartbeat", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto node_id = str(req.body);
    auto it = peer_addresses.find(node_id);
    cluster.heartbeat({node_id, it == peer_addresses.end() ? "remote" : it->second, true});
    return HttpResponse{200, "application/json", bytes("{\"ok\":true}\n")};
  });

  server.add_route("PUT", "/v1/chunks/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto checksum = req.path.substr(std::string("/v1/chunks/").size());
    auto ref = local_chunks.put(req.body);
    if (!ref.ok()) return status_response(ref.status());
    if (ref.value().checksum != checksum) return status_response(Status::Invalid("chunk checksum mismatch"));
    return HttpResponse{201, "application/json", bytes("{\"ok\":true}\n")};
  });

  server.add_route("GET", "/v1/chunks/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto checksum = req.path.substr(std::string("/v1/chunks/").size());
    auto data = local_chunks.get(checksum);
    if (!data.ok()) return status_response(data.status());
    return HttpResponse{200, "application/octet-stream", data.value()};
  });

  server.add_route("DELETE", "/v1/chunks/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto checksum = req.path.substr(std::string("/v1/chunks/").size());
    auto removed = local_chunks.remove_if_unreferenced(checksum, 0);
    if (!removed.ok()) return status_response(removed.status());
    return HttpResponse{204, "application/json", {}};
  });

  server.add_route("GET", "/metrics", [&](const HttpRequest&) {
    auto body = metrics.prometheus();
    return HttpResponse{200, "text/plain", bytes(body)};
  });

  server.add_route("GET", "/healthz", [&](const HttpRequest&) {
    return HttpResponse{200, "application/json", bytes("{\"ok\":true,\"node\":\"" + cfg.node_id + "\"}\n")};
  });

  server.add_route("POST", "/v1/admin/repair/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto bucket = req.path.substr(std::string("/v1/admin/repair/").size());
    auto repaired = repair_worker.repair_bucket(bucket);
    if (!repaired.ok()) return status_response(repaired.status());
    return HttpResponse{200, "application/json", bytes("{\"repaired\":" + std::to_string(repaired.value()) + "}\n")};
  });

  server.add_route("POST", "/v1/admin/rebalance/", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto bucket = req.path.substr(std::string("/v1/admin/rebalance/").size());
    auto moved = rebalance_worker.rebalance_bucket(bucket);
    if (!moved.ok()) return status_response(moved.status());
    return HttpResponse{200, "application/json", bytes("{\"rebalanced\":" + std::to_string(moved.value()) + "}\n")};
  });

  server.add_route("POST", "/v1/admin/handoff/replay", [&](const HttpRequest& req) {
    auto az = auth.authorize(bearer(req), Permission::kAdmin);
    if (!az.ok()) return status_response(az.status());
    auto delivered = chunks.replay_hinted_handoff();
    if (!delivered.ok()) return status_response(delivered.status());
    return HttpResponse{200, "application/json", bytes("{\"delivered\":" + std::to_string(delivered.value()) + "}\n")};
  });

  heartbeat_worker.start();
  auto run = server.run();
  if (!run.ok()) {
    DSTORE_LOG_ERROR("server", run.status().message());
    return 1;
  }
  return 0;
}
