#include "dstore/network/http_server.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

#include "dstore/common/logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#endif

namespace dstore {
namespace {

void close_socket(socket_t s) {
#ifdef _WIN32
  closesocket(s);
#else
  close(s);
#endif
}

class SocketRuntime {
 public:
  SocketRuntime() {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
  }
  ~SocketRuntime() {
#ifdef _WIN32
    WSACleanup();
#endif
  }
};

std::string reason(int status) {
  if (status == 200) return "OK";
  if (status == 201) return "Created";
  if (status == 204) return "No Content";
  if (status == 400) return "Bad Request";
  if (status == 401) return "Unauthorized";
  if (status == 403) return "Forbidden";
  if (status == 404) return "Not Found";
  if (status == 409) return "Conflict";
  return "Internal Server Error";
}

std::vector<std::uint8_t> serialize(const HttpResponse& r) {
  std::ostringstream head;
  head << "HTTP/1.1 " << r.status << " " << reason(r.status) << "\r\n";
  head << "Content-Type: " << r.content_type << "\r\n";
  head << "Content-Length: " << r.body.size() << "\r\n";
  head << "Connection: close\r\n\r\n";
  auto h = head.str();
  std::vector<std::uint8_t> out(h.begin(), h.end());
  out.insert(out.end(), r.body.begin(), r.body.end());
  return out;
}

Result<HttpRequest> parse_request(socket_t client) {
  std::string buffer;
  char tmp[8192];
  while (buffer.find("\r\n\r\n") == std::string::npos) {
    int n = recv(client, tmp, sizeof(tmp), 0);
    if (n <= 0) return Status::Invalid("failed to read request");
    buffer.append(tmp, tmp + n);
    if (buffer.size() > 1024 * 1024) return Status::Invalid("headers too large");
  }
  auto split = buffer.find("\r\n\r\n");
  std::string headers = buffer.substr(0, split);
  std::vector<std::uint8_t> body(buffer.begin() + static_cast<std::ptrdiff_t>(split + 4), buffer.end());
  std::istringstream in(headers);
  HttpRequest req;
  in >> req.method >> req.path;
  std::string line;
  std::getline(in, line);
  while (std::getline(in, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    auto key = line.substr(0, colon);
    auto value = line.substr(colon + 1);
    while (!value.empty() && value.front() == ' ') value.erase(value.begin());
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    req.headers[key] = value;
  }
  std::size_t content_length = 0;
  if (req.headers.contains("content-length")) content_length = static_cast<std::size_t>(std::stoull(req.headers["content-length"]));
  while (body.size() < content_length) {
    int n = recv(client, tmp, sizeof(tmp), 0);
    if (n <= 0) return Status::Invalid("failed to read body");
    body.insert(body.end(), tmp, tmp + n);
  }
  if (body.size() > content_length) body.resize(content_length);
  req.body = std::move(body);
  return req;
}

std::string route_key(const std::string& method, const std::string& prefix) { return method + " " + prefix; }

}  // namespace

HttpServer::HttpServer(std::string host, int port, std::size_t workers)
    : host_(std::move(host)), port_(port), pool_(workers) {}

void HttpServer::add_route(std::string method, std::string prefix, Handler handler) {
  routes_.push_back({std::move(method), std::move(prefix), std::move(handler)});
}

Result<void> HttpServer::run() {
  SocketRuntime runtime;
  socket_t server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) return Status::Internal("socket creation failed");
  int yes = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(port_));
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) return Status::Internal("bind failed");
  if (listen(server_fd, 256) < 0) return Status::Internal("listen failed");
  DSTORE_LOG_INFO("http", "listening on " + host_ + ":" + std::to_string(port_));
  while (true) {
    socket_t client = accept(server_fd, nullptr, nullptr);
    if (client < 0) continue;
    pool_.submit([this, client] {
      auto parsed = parse_request(client);
      HttpResponse response{400, "application/json", {'{','}','\n'}};
      if (parsed.ok()) {
        response = {404, "application/json", {'{','}','\n'}};
        for (const auto& [method, prefix, handler] : routes_) {
          if (parsed.value().method == method && parsed.value().path.rfind(prefix, 0) == 0) {
            response = handler(parsed.value());
            break;
          }
        }
      }
      auto bytes = serialize(response);
      send(client, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0);
      close_socket(client);
    });
  }
}

Result<HttpResponse> http_request(const std::string& host, int port, const HttpRequest& request) {
  SocketRuntime runtime;
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* resolved = nullptr;
  auto port_text = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &resolved) != 0) return Status::Unavailable("host resolution failed");
  socket_t fd = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
  if (fd < 0) {
    freeaddrinfo(resolved);
    return Status::Internal("socket creation failed");
  }
  if (connect(fd, resolved->ai_addr, static_cast<int>(resolved->ai_addrlen)) < 0) {
    freeaddrinfo(resolved);
    close_socket(fd);
    return Status::Unavailable("connect failed");
  }
  freeaddrinfo(resolved);
  std::ostringstream head;
  head << request.method << " " << request.path << " HTTP/1.1\r\n";
  head << "Host: " << host << "\r\n";
  for (const auto& [k, v] : request.headers) head << k << ": " << v << "\r\n";
  head << "Content-Length: " << request.body.size() << "\r\nConnection: close\r\n\r\n";
  auto h = head.str();
  send(fd, h.data(), static_cast<int>(h.size()), 0);
  if (!request.body.empty()) send(fd, reinterpret_cast<const char*>(request.body.data()), static_cast<int>(request.body.size()), 0);
  std::string raw;
  char buf[8192];
  int n = 0;
  while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) raw.append(buf, buf + n);
  close_socket(fd);
  auto split = raw.find("\r\n\r\n");
  if (split == std::string::npos) return Status::Internal("invalid response");
  std::istringstream status(raw.substr(0, raw.find("\r\n")));
  std::string http;
  HttpResponse resp;
  status >> http >> resp.status;
  resp.body.assign(raw.begin() + static_cast<std::ptrdiff_t>(split + 4), raw.end());
  return resp;
}

}  // namespace dstore
