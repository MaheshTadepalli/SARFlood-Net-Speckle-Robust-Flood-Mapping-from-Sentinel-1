#include <filesystem>
#include <fstream>
#include <iostream>

#include "dstore/network/http_server.h"

using namespace dstore;

namespace {
std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}
}  // namespace

int main(int argc, char** argv) {
  std::string host = "127.0.0.1";
  int port = 8080;
  std::string token = "admin-token";
  int i = 1;
  for (; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--host" && i + 1 < argc) host = argv[++i];
    else if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
    else if (arg == "--token" && i + 1 < argc) token = argv[++i];
    else break;
  }
  if (i >= argc) {
    std::cerr << "usage: dstore-cli [--host 127.0.0.1] [--port 8080] [--token token] upload|download|delete|metadata bucket/key [file]\n";
    return 2;
  }
  std::string cmd = argv[i++];
  std::string object = i < argc ? argv[i++] : "";
  HttpRequest req;
  req.headers["Authorization"] = "Bearer " + token;
  req.path = "/v1/objects/" + object;
  if (cmd == "upload") {
    if (i >= argc) return 2;
    req.method = "PUT";
    req.body = read_file(argv[i]);
  } else if (cmd == "download") {
    if (i >= argc) return 2;
    req.method = "GET";
  } else if (cmd == "delete") {
    req.method = "DELETE";
  } else if (cmd == "metadata") {
    req.method = "GET";
    req.path = "/v1/metadata/" + object;
  } else {
    return 2;
  }
  auto resp = http_request(host, port, req);
  if (!resp.ok()) {
    std::cerr << resp.status().message() << "\n";
    return 1;
  }
  if (resp.value().status >= 300) {
    std::cerr << std::string(resp.value().body.begin(), resp.value().body.end());
    return 1;
  }
  if (cmd == "download") write_file(argv[i], resp.value().body);
  else std::cout << std::string(resp.value().body.begin(), resp.value().body.end());
  return 0;
}
