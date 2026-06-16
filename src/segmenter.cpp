#include "tse/segmenter.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>

namespace tse {
namespace {

auto LowerAscii(std::string value) -> std::string {
  for (char &c : value) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return value;
}

auto Utf8Length(unsigned char c) -> std::size_t {
  if (c < 0x80)
    return 1;
  if ((c & 0xE0) == 0xC0)
    return 2;
  if ((c & 0xF0) == 0xE0)
    return 3;
  if ((c & 0xF8) == 0xF0)
    return 4;
  return 1;
}

void PushUnique(std::vector<std::string> &values, std::set<std::string> &seen,
                std::string value) {
  if (value.empty())
    return;
  if (seen.insert(value).second)
    values.push_back(std::move(value));
}

auto QuoteFtsToken(const std::string &token) -> std::string {
  auto quoted = std::string{"\""};
  for (const auto c : token) {
    if (c == '"')
      quoted += "\"\"";
    else
      quoted += c;
  }
  quoted += '"';
  return quoted;
}

} // namespace

auto Segmenter::Segment(std::string_view text) const
    -> std::vector<std::string> {
  auto tokens = std::vector<std::string>{};
  auto non_ascii_chars = std::vector<std::string>{};
  auto seen = std::set<std::string>{};

  for (auto i = std::size_t{0}; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    if (c < 0x80) {
      if (std::isalnum(c)) {
        auto end = i + 1;
        while (end < text.size() &&
               std::isalnum(static_cast<unsigned char>(text[end]))) {
          ++end;
        }
        PushUnique(tokens, seen,
                   LowerAscii(std::string(text.substr(i, end - i))));
        i = end;
      } else {
        ++i;
      }
      continue;
    }

    const auto len = std::min(Utf8Length(c), text.size() - i);
    auto ch = std::string(text.substr(i, len));
    non_ascii_chars.push_back(ch);
    PushUnique(tokens, seen, ch);
    i += len;
  }

  for (auto i = std::size_t{1}; i < non_ascii_chars.size(); ++i) {
    PushUnique(tokens, seen, non_ascii_chars[i - 1] + non_ascii_chars[i]);
  }

  return tokens;
}

auto Segmenter::BuildSearchText(std::string_view text) const -> std::string {
  auto out = std::ostringstream{};
  out << text << '\n';
  const auto tokens = Segment(text);
  for (const auto &token : tokens) {
    out << token << ' ';
  }
  return out.str();
}

auto Segmenter::BuildFtsQuery(const std::vector<std::string> &terms) const
    -> std::string {
  auto query = std::ostringstream{};
  auto first = true;
  for (const auto &term : terms) {
    if (term.empty())
      continue;
    if (!first)
      query << " OR ";
    query << QuoteFtsToken(term);
    first = false;
  }
  return query.str();
}

} // namespace tse
