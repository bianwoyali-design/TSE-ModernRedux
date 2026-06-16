#pragma once

#include "tse/types.hpp"

namespace tse {

class Crawler {
public:
  [[nodiscard]] auto Run(const CrawlOptions &options) const -> CrawlSummary;
};

} // namespace tse
