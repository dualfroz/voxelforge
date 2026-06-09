#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vf {

std::string base64Encode(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> base64Decode(const std::string& text);

} // namespace vf
