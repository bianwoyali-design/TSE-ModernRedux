#include "tse/html.hpp"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

namespace tse {
namespace {

auto Lower(std::string value) -> std::string {
  std::ranges::transform(value, value.begin(), [](unsigned char c) -> char {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

auto CollapseWhitespace(std::string_view value) -> std::string {
  auto result = std::string{};
  auto spaced = true;
  for (const auto c : value) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      if (!spaced)
        result += ' ';
      spaced = true;
    } else {
      result += c;
      spaced = false;
    }
  }
  if (!result.empty() && result.back() == ' ')
    result.pop_back();
  return result;
}

auto StripTags(std::string_view html) -> std::string {
  auto text = std::string{};
  auto in_tag = false;
  for (const auto c : html) {
    if (c == '<') {
      in_tag = true;
      text += ' ';
    } else if (c == '>') {
      in_tag = false;
    } else if (!in_tag) {
      text += c;
    }
  }
  return CollapseWhitespace(DecodeHtmlEntities(text));
}

auto AttributeValue(std::string_view tag, std::string_view name)
    -> std::string {
  const auto attr = std::regex(
      std::string(name) + R"REGEX(\s*=\s*("([^"]*)"|'([^']*)'|([^\s>]+)))REGEX",
      std::regex::icase);
  auto match = std::cmatch{};
  const auto tag_text = std::string(tag);
  if (!std::regex_search(tag_text.c_str(), match, attr))
    return {};
  if (match[2].matched)
    return DecodeHtmlEntities(match[2].str());
  if (match[3].matched)
    return DecodeHtmlEntities(match[3].str());
  return DecodeHtmlEntities(match[4].str());
}

auto ExtractTitleText(std::string_view html) -> std::string {
  const auto source = std::string(html);
  const auto title_re =
      std::regex(R"(<title[^>]*>([\s\S]*?)</title>)", std::regex::icase);
  auto match = std::smatch{};
  if (!std::regex_search(source, match, title_re))
    return {};
  return StripTags(match[1].str());
}

} // namespace

auto DecodeHtmlEntities(std::string_view text) -> std::string {
  auto result = std::string{};
  for (auto i = std::size_t{0}; i < text.size(); ++i) {
    if (text[i] != '&') {
      result += text[i];
      continue;
    }
    if (text.substr(i, 5) == "&amp;") {
      result += '&';
      i += 4;
    } else if (text.substr(i, 4) == "&lt;") {
      result += '<';
      i += 3;
    } else if (text.substr(i, 4) == "&gt;") {
      result += '>';
      i += 3;
    } else if (text.substr(i, 6) == "&quot;") {
      result += '"';
      i += 5;
    } else if (text.substr(i, 6) == "&nbsp;") {
      result += ' ';
      i += 5;
    } else if (text.substr(i, 5) == "&#39;") {
      result += '\'';
      i += 4;
    } else {
      result += '&';
    }
  }
  return result;
}

auto EscapeHtml(std::string_view text) -> std::string {
  auto result = std::string{};
  for (const auto c : text) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&#39;";
      break;
    default:
      result += c;
      break;
    }
  }
  return result;
}

auto HtmlToText(std::string_view html) -> std::string {
  auto filtered = std::string{};
  filtered.reserve(html.size());

  auto in_tag = false;
  auto in_script = false;
  auto in_style = false;
  auto tag = std::string{};

  for (auto i = std::size_t{0}; i < html.size(); ++i) {
    const auto c = html[i];
    if (c == '<') {
      in_tag = true;
      tag.clear();
      continue;
    }
    if (in_tag) {
      if (c == '>') {
        const auto lowered = Lower(tag);
        if (lowered.starts_with("script"))
          in_script = true;
        if (lowered.starts_with("/script"))
          in_script = false;
        if (lowered.starts_with("style"))
          in_style = true;
        if (lowered.starts_with("/style"))
          in_style = false;
        in_tag = false;
        filtered += ' ';
      } else {
        tag += c;
      }
      continue;
    }
    if (!in_script && !in_style)
      filtered += c;
  }
  return CollapseWhitespace(DecodeHtmlEntities(filtered));
}

auto ExtractHtml(std::string_view html) -> ExtractedPage {
  ExtractedPage page;
  page.title = ExtractTitleText(html);
  page.text = HtmlToText(html);

  auto pos = std::size_t{0};
  while ((pos = html.find('<', pos)) != std::string_view::npos) {
    const auto end = html.find('>', pos + 1);
    if (end == std::string_view::npos)
      break;
    const auto tag = std::string(html.substr(pos + 1, end - pos - 1));
    const auto lowered = Lower(tag);
    if (lowered.starts_with("a ") || lowered == "a" ||
        lowered.starts_with("area ") || lowered.starts_with("link ")) {
      auto link = Link{};
      link.url = AttributeValue(tag, "href");
      if (lowered.starts_with("a")) {
        const auto close =
            Lower(std::string(html.substr(end + 1))).find("</a>");
        if (close != std::string::npos) {
          link.anchor_text = StripTags(html.substr(end + 1, close));
        }
      }
      if (!link.url.empty())
        page.links.push_back(std::move(link));
    }
    pos = end + 1;
  }
  return page;
}

auto BuildSnippet(std::string_view text, const std::vector<std::string> &terms,
                  std::size_t max_length) -> std::string {
  const auto source = std::string(text);
  const auto lowered_source = Lower(source);
  auto best = std::size_t{0};
  for (const auto &term : terms) {
    if (term.empty())
      continue;
    const auto found = lowered_source.find(Lower(term));
    if (found != std::string::npos) {
      best = found > 60 ? found - 60 : 0;
      break;
    }
  }
  auto snippet = source.substr(best, max_length);
  if (best > 0)
    snippet = "..." + snippet;
  if (best + max_length < source.size())
    snippet += "...";
  return HighlightTerms(snippet, terms);
}

auto HighlightTerms(std::string_view text,
                    const std::vector<std::string> &terms) -> std::string {
  const auto source = std::string(text);
  const auto lowered_source = Lower(source);
  auto result = std::string{};
  for (auto i = std::size_t{0}; i < source.size();) {
    auto matched = std::size_t{0};
    for (const auto &term : terms) {
      if (term.empty() || term.size() <= matched ||
          i + term.size() > source.size())
        continue;
      if (lowered_source.compare(i, term.size(), Lower(term)) == 0)
        matched = term.size();
    }
    if (matched > 0) {
      result += "<mark>";
      result += EscapeHtml(std::string_view(source).substr(i, matched));
      result += "</mark>";
      i += matched;
    } else {
      result += EscapeHtml(std::string_view(source).substr(i, 1));
      ++i;
    }
  }
  return result;
}

} // namespace tse
