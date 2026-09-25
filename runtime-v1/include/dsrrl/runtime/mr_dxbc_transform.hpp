#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "mr_runtime_manifest.hpp"
#include "dsrrl/runtime/build151_mr_authority.hpp"

namespace dsrrl::runtime::mr {

enum class variant : std::uint8_t { diffuse_v29 = 0, full_v211 };

struct transform_result {
    bool ok = false;
    std::vector<std::uint8_t> code;
    std::string error;
};

const plan *find_plan(std::size_t size, std::string_view sha256) noexcept;
const build151::lerp_plan *find_lerp_plan(std::size_t size, std::string_view sha256) noexcept;
transform_result transform(std::span<const std::uint8_t> stock, const plan &p, variant v);
transform_result transform_lerp(
    std::span<const std::uint8_t> stock,
    const build151::lerp_plan &p,
    variant v);

// Preserve the exact V2.10/V2.11 spec-material corrections while reverting
// only V29 diffuse c100/domain changes back to the stock DSR diffuse lane.
// This is the source-complete equivalent of the historical SPEC_ONLY class.
transform_result transform_stock_diffuse_material(
    std::span<const std::uint8_t> v211,
    const plan &p);
transform_result transform_stock_diffuse_material_lerp(
    std::span<const std::uint8_t> v211,
    const build151::lerp_plan &p);

transform_result transform_v9a(std::span<const std::uint8_t> v211);
transform_result transform_upper_lower(
    std::span<const std::uint8_t> base,
    std::string_view expected_output_sha256 = {});
transform_result transform_spec_rgb(std::span<const std::uint8_t> base);
transform_result transform_pmetal_v13(std::span<const std::uint8_t> base);

} // namespace dsrrl::runtime::mr
