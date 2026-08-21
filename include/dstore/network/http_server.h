#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "dstore/common/result.h"
#include "dstore/common/thread_pool.h"

namespace dstore {

struct HttpRequest {
  std::string method;
  std::string path;
  std::unordered_map<std::string, std::string> headers;
  std::vector<std::uint8_t> body;
};

struct HttpResponse {
  int status = 200;
  std::string content_type = "application/json";
  std::vector<std::uint8_t> body;
};

class HttpServer {
 public:
  using Handler = std::function<HttpResponse(const HttpRequest&)>;
  HttpServer(std::string host, int port, std::size_t workers);
  void add_route(std::string method, std::string prefix, Handler handler);
  Result<void> run();

 private:
  std::string host_;
  int port_;
  ThreadPool pool_;
  std::vector<std::tuple<std::string, std::string, Handler>> routes_;
};

Result<HttpResponse> http_request(const std::string& host, int port, const HttpRequest& request);

}  // namespace dstore
