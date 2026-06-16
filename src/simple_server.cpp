#include "tse/simple_server.hpp"

#include <array>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tse/url.hpp"

namespace tse {
namespace {

class UniqueFd {
public:
  explicit UniqueFd(int fd = -1) : fd_(fd) {}
  ~UniqueFd() {
    if (fd_ >= 0)
      close(fd_);
  }

  UniqueFd(const UniqueFd &) = delete;
  auto operator=(const UniqueFd &) -> UniqueFd & = delete;

  [[nodiscard]] auto get() const -> int { return fd_; }
  auto release() -> int {
    const auto fd = fd_;
    fd_ = -1;
    return fd;
  }

private:
  int fd_;
};

auto StatusText(int status) -> std::string {
  switch (status) {
  case 200:
    return "OK";
  case 400:
    return "Bad Request";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  default:
    return "Unknown";
  }
}

auto SerializeResponse(const HttpResponse &response) -> std::string {
  auto out = std::ostringstream{};
  out << "HTTP/1.0 " << response.status << ' ' << StatusText(response.status)
      << "\r\n";
  out << "Content-Type: " << response.content_type << "\r\n";
  out << "Content-Length: " << response.body.size() << "\r\n";
  out << "Connection: close\r\n";
  out << "Access-Control-Allow-Origin: *\r\n\r\n";
  out << response.body;
  return out.str();
}

auto ContentTypeFor(const std::filesystem::path &path) -> std::string {
  const auto ext = path.extension().string();
  if (ext == ".css")
    return "text/css; charset=utf-8";
  if (ext == ".js")
    return "application/javascript; charset=utf-8";
  if (ext == ".html")
    return "text/html; charset=utf-8";
  if (ext == ".jpg" || ext == ".jpeg")
    return "image/jpeg";
  if (ext == ".gif")
    return "image/gif";
  if (ext == ".png")
    return "image/png";
  return "application/octet-stream";
}

} // namespace

SimpleServer::SimpleServer(std::uint16_t port, std::string web_root)
    : port_(port), web_root_(std::move(web_root)) {}

void SimpleServer::AddRoute(std::string prefix, RouteHandler handler) {
  routes_[std::move(prefix)] = std::move(handler);
}

void SimpleServer::Run() {
  auto server_fd = UniqueFd{socket(AF_INET, SOCK_STREAM, 0)};
  if (server_fd.get() < 0)
    throw std::runtime_error("failed to create socket");

  auto opt = 1;
  setsockopt(server_fd.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  auto addr = sockaddr_in{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port_);

  if (bind(server_fd.get(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    throw std::runtime_error("failed to bind port " + std::to_string(port_));
  }
  if (listen(server_fd.get(), 16) < 0)
    throw std::runtime_error("failed to listen");

  std::cout << "TSE server listening on http://localhost:" << port_ << "/\n";
  while (true) {
    auto client_addr = sockaddr_in{};
    auto client_len = static_cast<socklen_t>(sizeof(client_addr));
    auto client = UniqueFd{accept(server_fd.get(),
                                  reinterpret_cast<sockaddr *>(&client_addr),
                                  &client_len)};
    if (client.get() < 0)
      continue;
    HandleClient(client.get());
  }
}

void SimpleServer::HandleClient(int client_fd) {
  auto buffer = std::array<char, 16384>{};
  const auto n = recv(client_fd, buffer.data(), buffer.size(), 0);
  if (n <= 0)
    return;

  const auto request = ParseRequest(
      std::string_view(buffer.data(), static_cast<std::size_t>(n)));
  auto response = HttpResponse{};

  auto handled = false;
  for (const auto &[prefix, handler] : routes_) {
    if (request.path.starts_with(prefix)) {
      handler(request, response);
      handled = true;
      break;
    }
  }
  if (!handled)
    ServeStaticFile(request.path, response);

  const auto serialized = SerializeResponse(response);
  send(client_fd, serialized.data(), serialized.size(), 0);
}

auto SimpleServer::ParseRequest(std::string_view raw) const -> HttpRequest {
  auto request = HttpRequest{};
  const auto line_end = raw.find("\r\n");
  if (line_end == std::string_view::npos)
    return request;

  const auto first = raw.substr(0, line_end);
  const auto sp1 = first.find(' ');
  const auto sp2 = first.rfind(' ');
  if (sp1 == std::string_view::npos || sp2 == std::string_view::npos ||
      sp2 <= sp1)
    return request;

  request.method = std::string(first.substr(0, sp1));
  auto full_path = std::string(first.substr(sp1 + 1, sp2 - sp1 - 1));
  const auto query = full_path.find('?');
  if (query == std::string::npos) {
    request.path = UrlDecode(full_path);
    return request;
  }

  request.path = UrlDecode(std::string_view(full_path).substr(0, query));
  auto query_text = std::string_view(full_path.data() + query + 1,
                                     full_path.size() - query - 1);
  auto pos = std::size_t{0};
  while (pos < query_text.size()) {
    const auto amp = query_text.find('&', pos);
    const auto end = amp == std::string_view::npos ? query_text.size() : amp;
    const auto eq = query_text.find('=', pos);
    if (eq != std::string_view::npos && eq < end) {
      request.params[UrlDecode(query_text.substr(pos, eq - pos))] =
          UrlDecode(query_text.substr(eq + 1, end - eq - 1));
    }
    if (amp == std::string_view::npos)
      break;
    pos = amp + 1;
  }
  return request;
}

void SimpleServer::ServeStaticFile(const std::string &path,
                                   HttpResponse &response) const {
  if (path.find("..") != std::string::npos) {
    response.status = 404;
    response.body = "Not Found";
    return;
  }

  auto file = std::filesystem::path{web_root_};
  file /= path == "/" ? "index.html" : path.substr(1);
  if (!std::filesystem::is_regular_file(file)) {
    response.status = 404;
    response.body = "Not Found";
    return;
  }

  auto input = std::ifstream{file, std::ios::binary};
  auto body = std::ostringstream{};
  body << input.rdbuf();
  response.body = body.str();
  response.content_type = ContentTypeFor(file);
}

} // namespace tse
