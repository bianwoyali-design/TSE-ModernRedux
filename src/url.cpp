#include "tse/url.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace tse {
namespace {

auto Lower(std::string value) -> std::string {
  std::ranges::transform(value, value.begin(), [](unsigned char c) -> char {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

auto IsSupportedScheme(std::string_view scheme) -> bool {
  return scheme == "http" || scheme == "https";
}

auto DefaultPort(std::string_view scheme) -> int {
  if (scheme == "https")
    return 443;
  return 80;
}

auto RemoveDotSegments(std::string_view path) -> std::string {
  auto parts = std::vector<std::string>{};
  auto pos = std::size_t{0};
  while (pos <= path.size()) {
    const auto slash = path.find('/', pos);
    const auto end = slash == std::string_view::npos ? path.size() : slash;
    auto part = std::string(path.substr(pos, end - pos));
    if (part == "..") {
      if (!parts.empty())
        parts.pop_back();
    } else if (!part.empty() && part != ".") {
      parts.push_back(std::move(part));
    }
    if (slash == std::string_view::npos)
      break;
    pos = slash + 1;
  }

  auto result = std::string{"/"};
  for (auto i = std::size_t{0}; i < parts.size(); ++i) {
    if (i > 0)
      result += '/';
    result += parts[i];
  }
  if (!path.empty() && path.back() == '/' && result.back() != '/') {
    result += '/';
  }
  return result;
}

auto ParentPath(std::string_view path) -> std::string {
  if (path.empty() || path[0] != '/')
    return "/";
  const auto slash = path.rfind('/');
  if (slash == std::string_view::npos || slash == 0)
    return "/";
  return std::string(path.substr(0, slash + 1));
}

auto IsHex(char c) -> bool {
  return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

} // namespace

auto ParseUrl(std::string_view raw_url) -> std::optional<ParsedUrl> {
  auto input = std::string(raw_url);
  input.erase(0, input.find_first_not_of(" \t\r\n"));
  input.erase(input.find_last_not_of(" \t\r\n") + 1);
  const auto fragment = input.find('#');
  if (fragment != std::string::npos)
    input.erase(fragment);

  const auto scheme_end = input.find("://");
  if (scheme_end == std::string::npos)
    return std::nullopt;

  auto parsed = ParsedUrl{};
  parsed.scheme = Lower(input.substr(0, scheme_end));
  if (!IsSupportedScheme(parsed.scheme))
    return std::nullopt;

  const auto authority_start = scheme_end + 3;
  const auto path_start = input.find_first_of("/?", authority_start);
  auto authority =
      path_start == std::string::npos
          ? input.substr(authority_start)
          : input.substr(authority_start, path_start - authority_start);
  if (authority.empty())
    return std::nullopt;

  const auto at = authority.rfind('@');
  if (at != std::string::npos)
    authority.erase(0, at + 1);

  parsed.port = DefaultPort(parsed.scheme);
  if (!authority.empty() && authority.front() == '[') {
    const auto end = authority.find(']');
    if (end == std::string::npos)
      return std::nullopt;
    parsed.host = Lower(authority.substr(0, end + 1));
    if (end + 1 < authority.size() && authority[end + 1] == ':') {
      auto port_text = std::string_view(authority).substr(end + 2);
      std::from_chars(port_text.data(), port_text.data() + port_text.size(),
                      parsed.port);
    }
  } else {
    const auto colon = authority.rfind(':');
    if (colon != std::string::npos) {
      parsed.host = Lower(authority.substr(0, colon));
      auto port_text = std::string_view(authority).substr(colon + 1);
      if (port_text.empty())
        return std::nullopt;
      auto result = std::from_chars(
          port_text.data(), port_text.data() + port_text.size(), parsed.port);
      if (result.ec != std::errc{})
        return std::nullopt;
    } else {
      parsed.host = Lower(authority);
    }
  }
  if (parsed.host.empty())
    return std::nullopt;

  auto path_and_query = path_start == std::string::npos
                            ? std::string{"/"}
                            : input.substr(path_start);
  const auto query_start = path_and_query.find('?');
  if (query_start == std::string::npos) {
    parsed.path = path_and_query.empty() ? "/" : path_and_query;
  } else {
    parsed.path =
        query_start == 0 ? "/" : path_and_query.substr(0, query_start);
    parsed.query = path_and_query.substr(query_start + 1);
  }
  parsed.path = RemoveDotSegments(parsed.path.empty() ? "/" : parsed.path);

  auto normalized = std::ostringstream{};
  normalized << parsed.scheme << "://" << parsed.host;
  if (parsed.port != DefaultPort(parsed.scheme))
    normalized << ':' << parsed.port;
  normalized << parsed.path;
  if (!parsed.query.empty())
    normalized << '?' << parsed.query;
  parsed.normalized = normalized.str();
  return parsed;
}

auto NormalizeUrl(std::string_view raw_url) -> std::optional<std::string> {
  auto parsed = ParseUrl(raw_url);
  if (!parsed)
    return std::nullopt;
  return parsed->normalized;
}

auto ResolveUrl(std::string_view base_url, std::string_view href)
    -> std::optional<std::string> {
  auto link = std::string(href);
  link.erase(0, link.find_first_not_of(" \t\r\n"));
  link.erase(link.find_last_not_of(" \t\r\n") + 1);
  if (link.empty() || link.front() == '#')
    return std::nullopt;

  const auto lower = Lower(link);
  if (lower.starts_with("javascript:") || lower.starts_with("mailto:") ||
      lower.starts_with("tel:")) {
    return std::nullopt;
  }
  if (lower.starts_with("http://") || lower.starts_with("https://")) {
    return NormalizeUrl(link);
  }

  auto base = ParseUrl(base_url);
  if (!base)
    return std::nullopt;

  if (link.starts_with("//")) {
    return NormalizeUrl(base->scheme + ":" + link);
  }

  auto combined = std::string{};
  if (link.starts_with('/')) {
    combined = base->scheme + "://" + base->host;
    if (base->port != DefaultPort(base->scheme))
      combined += ":" + std::to_string(base->port);
    combined += link;
  } else {
    combined = base->scheme + "://" + base->host;
    if (base->port != DefaultPort(base->scheme))
      combined += ":" + std::to_string(base->port);
    combined += ParentPath(base->path);
    combined += link;
  }
  return NormalizeUrl(combined);
}

auto UrlDecode(std::string_view value) -> std::string {
  auto result = std::string{};
  result.reserve(value.size());
  for (auto i = std::size_t{0}; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size() && IsHex(value[i + 1]) &&
        IsHex(value[i + 2])) {
      auto decoded = 0;
      auto bytes = value.substr(i + 1, 2);
      std::from_chars(bytes.data(), bytes.data() + bytes.size(), decoded, 16);
      result += static_cast<char>(decoded);
      i += 2;
    } else if (value[i] == '+') {
      result += ' ';
    } else {
      result += value[i];
    }
  }
  return result;
}

auto UrlEncode(std::string_view value) -> std::string {
  static constexpr char kHex[] = "0123456789ABCDEF";
  auto result = std::string{};
  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      result += static_cast<char>(c);
    } else {
      result += '%';
      result += kHex[c >> 4];
      result += kHex[c & 0x0F];
    }
  }
  return result;
}

} // namespace tse
