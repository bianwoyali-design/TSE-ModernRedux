#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace tse {

class Segmenter {
public:
  [[nodiscard]] auto Segment(std::string_view text) const
      -> std::vector<std::string>;
  [[nodiscard]] auto BuildSearchText(std::string_view text) const
      -> std::string;
  [[nodiscard]] auto BuildFtsQuery(const std::vector<std::string> &terms) const
      -> std::string;
};

} // namespace tse
