#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tse {

struct ParsedUrl {
  std::string scheme;
  std::string host;
  int port = 0;
  std::string path;
  std::string query;
  std::string normalized;
};

auto ParseUrl(std::string_view raw_url) -> std::optional<ParsedUrl>;
auto NormalizeUrl(std::string_view raw_url) -> std::optional<std::string>;
auto ResolveUrl(std::string_view base_url, std::string_view href)
    -> std::optional<std::string>;
auto UrlDecode(std::string_view value) -> std::string;
auto UrlEncode(std::string_view value) -> std::string;

} // namespace tse
