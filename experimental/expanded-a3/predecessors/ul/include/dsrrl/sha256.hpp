#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace dsrrl {
using sha256_digest = std::array<std::uint8_t, 32>;
sha256_digest sha256(std::span<const std::byte> data);
sha256_digest sha256_file(const std::filesystem::path &path);
std::string to_hex(const sha256_digest &digest);
bool parse_sha256(std::string_view text, sha256_digest &out);
}
