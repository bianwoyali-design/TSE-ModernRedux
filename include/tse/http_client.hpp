#pragma once

#include <map>
#include <string>

namespace tse {

struct HttpFetchResult {
  bool ok = false;
  int status_code = 0;
  std::string final_url;
  std::string content_type;
  std::string body;
  std::string error;
  std::map<std::string, std::string> headers;
};

class HttpClient {
public:
  [[nodiscard]] auto Fetch(const std::string &url, int redirect_limit = 5) const
      -> HttpFetchResult;
};

} // namespace tse
