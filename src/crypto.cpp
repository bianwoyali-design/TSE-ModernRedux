#include "tse/crypto.hpp"

#include <array>
#include <iomanip>
#include <sstream>

#include <openssl/evp.h>

namespace tse {

auto Sha256Hex(std::string_view value) -> std::string {
  auto digest = std::array<unsigned char, EVP_MAX_MD_SIZE>{};
  auto digest_len = 0U;

  auto *ctx = EVP_MD_CTX_new();
  if (!ctx)
    return {};

  EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
  EVP_DigestUpdate(ctx, value.data(), value.size());
  EVP_DigestFinal_ex(ctx, digest.data(), &digest_len);
  EVP_MD_CTX_free(ctx);

  auto out = std::ostringstream{};
  for (auto i = 0U; i < digest_len; ++i) {
    out << std::hex << std::setw(2) << std::setfill('0')
        << static_cast<int>(digest[i]);
  }
  return out.str();
}

} // namespace tse
