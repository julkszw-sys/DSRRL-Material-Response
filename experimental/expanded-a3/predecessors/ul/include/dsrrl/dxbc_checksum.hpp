#pragma once
#include <cstdint>
#include <vector>
namespace dsrrl {
bool fix_dxbc_checksum(std::vector<std::uint8_t> &bytes) noexcept;
}
