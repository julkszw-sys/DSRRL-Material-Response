#pragma once

#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::point_light {

enum class clustered_spc_legacy_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_spc,
    fail_invalid_dxbc,
    fail_contract,
    fail_material_capture,
    fail_declaration_shape,
    fail_island_emit,
    fail_rebuild,
    fail_postcondition
};

struct clustered_spc_legacy_materialize_outcome {
    clustered_spc_legacy_materialize_result result =
        clustered_spc_legacy_materialize_result::pass_not_spc;
    core::sha256_digest replacement_sha256{};
    std::size_t replacement_size = 0u;
    std::uint32_t representative_shader_index = 0u;
    std::uint32_t color0_input_register = 0u;
    std::uint32_t original_temp_count = 0u;
    std::uint32_t final_temp_count = 0u;
    bool blended_material = false;
    bool t10_declared = false;
    bool t16_declared = false;
    bool stock_microfacet_killed_at_output_cut = false;
    bool common_ndotl_specular_bypassed = false;
    bool ptde_legacy_specular_added = false;
};

// Second create-time stage for exact-hash clustered Spc PntS hosts.
//
// Input is the already-attested direct clustered PointLight replacement
// produced by materialize_clustered_pnts_direct_ptde(). The stage keeps that
// replacement's source/membership/linear-attenuation/diffuse path, kills the
// DSR GGX/Schlick specular value at the local-light composition cut, and adds
// a separate PTDE legacy specular branch:
//
//   Cspec = Csrc * A * pow(max(R dot L, 0), c102)
//           * (PTDE_SpecRGB_blend * c101 * COLOR0)
//
// Crucially the added specular branch is NOT multiplied by the host N dot L.
// Exact PTDE SpecRGB is sampled from draw-local t10 (and t16 for blended
// materials); b12[2].xyz carries c101 and b12[0].w carries c102.
//
// Unknown/malformed shader structure fails open.
clustered_spc_legacy_materialize_outcome
materialize_clustered_spc_legacy_specular(
    const std::uint8_t *stage1,
    std::size_t size,
    bool spc,
    bool blended_material,
    std::uint32_t representative_shader_index,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
