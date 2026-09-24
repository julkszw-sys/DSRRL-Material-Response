#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "mr_runtime_manifest.hpp"

namespace dsrrl::runtime::mr {

enum class variant : std::uint8_t {
    diffuse_v29 = 0,
    full_v211,
    diffuse_v29_specrgb,
    full_v211_specrgb
};

struct transform_result {
    bool ok = false;
    std::vector<std::uint8_t> code;
    std::string error;
};

const plan *find_plan(std::size_t size, std::string_view sha256) noexcept;
transform_result transform(std::span<const std::uint8_t> stock, const plan &p, variant v);

} // namespace dsrrl::runtime::mr
