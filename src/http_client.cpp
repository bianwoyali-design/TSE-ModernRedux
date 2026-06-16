#include "tse/http_client.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <netdb.h>
#include <openssl/ssl.h>
#include <sys/socket.h>
#include <unistd.h>

#include "tse/url.hpp"

namespace tse {
namespace {

struct UniqueFd {
  int fd = -1;
  ~UniqueFd() {
    if (fd >= 0)
      close(fd);
  }
};

auto Lower(std::string value) -> std::string {
  std::ranges::transform(value, value.begin(), [](unsigned char c) -> char {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

auto ConnectTcp(const std::string &host, int port) -> int {
  auto hints = addrinfo{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_UNSPEC;

  auto *raw = static_cast<addrinfo *>(nullptr);
  const auto port_text = std::to_string(port);
  if (getaddrinfo(host.c_str(), port_text.c_str(), &hints, &raw) != 0)
    return -1;
  auto results =
      std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>{raw, freeaddrinfo};

  for (auto *ai = results.get(); ai; ai = ai->ai_next) {
    auto fd = UniqueFd{socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)};
    if (fd.fd < 0)
      continue;
    if (connect(fd.fd, ai->ai_addr, ai->ai_addrlen) == 0) {
      const auto connected = fd.fd;
      fd.fd = -1;
      return connected;
    }
  }
  return -1;
}

void ParseResponse(HttpFetchResult &result, const std::string &raw) {
  auto header_end = raw.find("\r\n\r\n");
  auto delimiter = std::size_t{4};
  if (header_end == std::string::npos) {
    header_end = raw.find("\n\n");
    delimiter = 2;
  }
  if (header_end == std::string::npos) {
    result.error = "HTTP response has no header delimiter";
    return;
  }

  const auto header_block = raw.substr(0, header_end);
  result.body = raw.substr(header_end + delimiter);

  auto headers = std::istringstream{header_block};
  auto line = std::string{};
  if (std::getline(headers, line)) {
    auto status = std::istringstream{line};
    auto http_version = std::string{};
    status >> http_version >> result.status_code;
  }

  while (std::getline(headers, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const auto colon = line.find(':');
    if (colon == std::string::npos)
      continue;
    auto key = Lower(line.substr(0, colon));
    auto value = line.substr(colon + 1);
    value.erase(0, value.find_first_not_of(" \t"));
    result.headers[key] = value;
    if (key == "content-type")
      result.content_type = value;
  }
}

auto DecodeChunked(std::string_view body) -> std::string {
  auto decoded = std::string{};
  auto pos = std::size_t{0};
  while (pos < body.size()) {
    const auto end = body.find("\r\n", pos);
    if (end == std::string::npos)
      break;
    const auto size_text = std::string(body.substr(pos, end - pos));
    const auto semi = size_text.find(';');
    const auto hex_size =
        semi == std::string::npos ? size_text : size_text.substr(0, semi);
    auto chunk_size = std::size_t{0};
    std::istringstream(hex_size) >> std::hex >> chunk_size;
    if (chunk_size == 0)
      break;
    pos = end + 2;
    if (pos + chunk_size > body.size())
      break;
    decoded.append(body.substr(pos, chunk_size));
    pos += chunk_size + 2;
  }
  return decoded;
}

} // namespace

auto HttpClient::Fetch(const std::string &url, int redirect_limit) const
    -> HttpFetchResult {
  auto result = HttpFetchResult{};
  auto parsed = ParseUrl(url);
  if (!parsed) {
    result.error = "unsupported or invalid URL";
    return result;
  }

  auto fd = UniqueFd{ConnectTcp(parsed->host, parsed->port)};
  if (fd.fd < 0) {
    result.error = "TCP connect failed";
    return result;
  }

  const auto target =
      parsed->path + (parsed->query.empty() ? "" : "?" + parsed->query);
  const auto request = "GET " + target +
                       " HTTP/1.1\r\n"
                       "Host: " +
                       parsed->host +
                       "\r\n"
                       "User-Agent: TSE/3.0\r\n"
                       "Accept: text/html,application/xhtml+xml,*/*;q=0.8\r\n"
                       "Accept-Encoding: identity\r\n"
                       "Connection: close\r\n\r\n";

  auto raw_response = std::string{};
  auto buffer = std::array<char, 8192>{};

  if (parsed->scheme == "https") {
    SSL_library_init();
    const auto *method = TLS_client_method();
    auto ctx = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>{
        SSL_CTX_new(method), SSL_CTX_free};
    if (!ctx) {
      result.error = "SSL context creation failed";
      return result;
    }
    auto ssl =
        std::unique_ptr<SSL, decltype(&SSL_free)>{SSL_new(ctx.get()), SSL_free};
    SSL_set_tlsext_host_name(ssl.get(), parsed->host.c_str());
    SSL_set_fd(ssl.get(), fd.fd);
    if (SSL_connect(ssl.get()) <= 0) {
      result.error = "SSL connect failed";
      return result;
    }
    if (SSL_write(ssl.get(), request.data(),
                  static_cast<int>(request.size())) <= 0) {
      result.error = "SSL write failed";
      return result;
    }
    while (true) {
      const auto n =
          SSL_read(ssl.get(), buffer.data(), static_cast<int>(buffer.size()));
      if (n <= 0)
        break;
      raw_response.append(buffer.data(), n);
    }
  } else {
    if (send(fd.fd, request.data(), request.size(), 0) < 0) {
      result.error = "socket write failed";
      return result;
    }
    while (true) {
      const auto n = recv(fd.fd, buffer.data(), buffer.size(), 0);
      if (n <= 0)
        break;
      raw_response.append(buffer.data(), static_cast<std::size_t>(n));
    }
  }

  ParseResponse(result, raw_response);
  result.final_url = parsed->normalized;
  if (Lower(result.headers["transfer-encoding"]).find("chunked") !=
      std::string::npos) {
    result.body = DecodeChunked(result.body);
  }

  if (result.status_code >= 300 && result.status_code < 400 &&
      redirect_limit > 0) {
    const auto it = result.headers.find("location");
    if (it != result.headers.end()) {
      if (auto next_url = ResolveUrl(parsed->normalized, it->second)) {
        return Fetch(*next_url, redirect_limit - 1);
      }
    }
  }

  result.ok = result.status_code >= 200 && result.status_code < 300;
  if (!result.ok && result.error.empty())
    result.error = "HTTP status " + std::to_string(result.status_code);
  return result;
}

} // namespace tse
