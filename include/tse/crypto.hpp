#pragma once

#include <string>
#include <string_view>

namespace tse {

auto Sha256Hex(std::string_view value) -> std::string;

} // namespace tse
