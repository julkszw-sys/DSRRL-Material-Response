#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "mr_runtime_manifest.hpp"
#include "ul_runtime_manifest.hpp"

namespace dsrrl::runtime::mr {

enum class variant : std::uint8_t { diffuse_v29 = 0, full_v211 };

struct transform_result {
    bool ok = false;
    std::vector<std::uint8_t> code;
    std::string error;
};

const plan *find_plan(std::size_t size, std::string_view sha256) noexcept;
transform_result transform(std::span<const std::uint8_t> stock, const plan &p, variant v);

const ul_plan *find_ul_plan(std::string_view sha256) noexcept;

// Local PTDE U/L consumer island. Rewrites only the canonical hemisphere
// references cb0[7]/cb0[8] -> b13[6]/b13[7], declares b13[8], strips stale
// RDEF and recomputes the DXBC checksum. It may be applied to stock DXBC or
// to an already-transformed MR V2.11 DXBC; no historical generated blob is
// required.
transform_result transform_upper_lower(std::span<const std::uint8_t> base);

} // namespace dsrrl::runtime::mr
