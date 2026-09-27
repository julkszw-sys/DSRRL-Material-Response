#pragma once

#include "dsrrl/core/feature_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::point_light {

enum class fixed_local_single_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_fixed_local_specular,
    pass_blended_requires_endpoint_b,
    pass_a1_not_fully_materialized,
    fail_invalid_dxbc,
    fail_contract,
    fail_color0_signature,
    fail_declaration_shape,
    fail_material_capture,
    fail_geometry_capture,
    fail_island_emit,
    fail_rebuild,
    fail_postcondition
};

struct fixed_local_single_materialize_outcome {
    fixed_local_single_materialize_result result =
        fixed_local_single_materialize_result::
            pass_not_fixed_local_specular;
    std::uint8_t light_count = 0u;
    std::uint32_t color0_input_register = 0u;
    std::uint32_t original_temp_count = 0u;
    std::uint32_t final_temp_count = 0u;
    bool raw_host_basis = false;
    bool rdef_stripped = false;
    bool t10_declared = false;
    bool t19_declared = false;
    bool t16_declared = false;
    bool b12_declared = false;
    bool blended_material = false;
    bool output_cut_redirected = false;

    // Exact created-code attestation seam. The fixed 48-body corpus is not
    // part of the A1 recipe set, so the replacement is generated directly
    // from the byte-identical stock DSR fixed host. Runtime may associate the
    // replacement only when init_pipeline exposes that exact SHA256 + size.
    std::array<std::uint8_t,32> host_sha256{};
    std::size_t host_size = 0u;
    std::array<std::uint8_t,32> replacement_sha256{};
};

// Exact fixed Spc PntSS/PntSSSS materializer. Historical function name is
// retained for ABI/source compatibility; it now supports both the 24
// single-endpoint bodies and the 24 Mul/blended bodies once endpoint-B routing
// is proven by the exact material-sample contract.
//
// Pipeline:
//   exact original DSR identity (raw fixed host; A1 corpus is disjoint)
 //   -> t10 PTDE SpecRGB sample into isolated scratch RGB
 //      (stock t1/t4, including any alpha lane, remain untouched)
//   -> for Mul: t16 split from t4 + exact per-body A/B blend operand
//   -> raw PTDE material carriers from b12[1]/b12[2]
//   -> raw-q t19 + PTDE legacy fixed PointLight island
//   -> common cb0[139] multiplier
//   -> redirect immediate post-ENDIF ADD src1
//   -> strip RDEF + rebuild checksum.
//
// Diffuse Mul uses the audited post-cb156 stock blend seam; SpecRGB Mul uses
// PTDE sidecars t10/t16 and the exact shared blend operand.
fixed_local_single_materialize_outcome
materialize_fixed_local_specular_single(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
