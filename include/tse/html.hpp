#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "tse/types.hpp"

namespace tse {

struct ExtractedPage {
  std::string title;
  std::string text;
  std::vector<Link> links;
};

auto ExtractHtml(std::string_view html) -> ExtractedPage;
auto HtmlToText(std::string_view html) -> std::string;
auto DecodeHtmlEntities(std::string_view text) -> std::string;
auto EscapeHtml(std::string_view text) -> std::string;
auto HighlightTerms(std::string_view text,
                    const std::vector<std::string> &terms) -> std::string;
auto BuildSnippet(std::string_view text, const std::vector<std::string> &terms,
                  std::size_t max_length = 240) -> std::string;

} // namespace tse
