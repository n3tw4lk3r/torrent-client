#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace utils {

std::int32_t bytes_to_int32_t(std::string_view bytes);
std::string int32_t_to_bytes(std::int32_t value);
std::string uint64_t_to_bytes(std::uint64_t value);
std::uint64_t bytes_to_uint64_t(std::string_view bytes);

std::string calculate_sha1(std::string_view msg);
std::string hex_encode(std::string_view input);
std::string bytes_to_hex(std::string_view bytes);

} // namespace utils

