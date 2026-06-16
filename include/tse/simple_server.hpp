#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace tse {

struct HttpRequest {
  std::string method;
  std::string path;
  std::map<std::string, std::string> params;
};

struct HttpResponse {
  int status = 200;
  std::string content_type = "text/html; charset=utf-8";
  std::string body;
};

using RouteHandler = std::function<void(const HttpRequest &, HttpResponse &)>;

class SimpleServer {
public:
  SimpleServer(std::uint16_t port, std::string web_root);
  void AddRoute(std::string prefix, RouteHandler handler);
  void Run();

private:
  std::uint16_t port_;
  std::string web_root_;
  std::map<std::string, RouteHandler> routes_;

  void HandleClient(int client_fd);
  [[nodiscard]] auto ParseRequest(std::string_view raw) const -> HttpRequest;
  void ServeStaticFile(const std::string &path, HttpResponse &response) const;
};

} // namespace tse
