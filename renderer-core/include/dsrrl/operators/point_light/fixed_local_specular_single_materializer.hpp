#pragma once

#include "dsrrl/core/feature_registry.hpp"

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
    bool a1_full_plan = false;
    bool rdef_stripped = false;
    bool t10_declared = false;
    bool t19_declared = false;
    bool b12_declared = false;
    bool output_cut_redirected = false;
};

// Construction-only exact materializer for the 24 unique single-endpoint
// fixed Spc PntSS/PntSSSS bodies.
//
// Pipeline:
//   exact original DSR identity
//   -> full enabled A1/P2.2 materialization
//   -> t10 split from t1 with stock alpha preserved
//   -> raw PTDE material carriers from b12[1]/b12[2]
//   -> raw-q t19 + PTDE legacy fixed PointLight island
//   -> common cb0[139] multiplier
//   -> redirect immediate post-ENDIF ADD src1
//   -> strip RDEF + rebuild checksum.
//
// Mul/blended t3/t4 endpoint-B bodies deliberately fail open until the second
// exact PTDE diffuse/spec resource route is available.
fixed_local_single_materialize_outcome
materialize_fixed_local_specular_single(
    const core::feature_registry &features,
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::point_light
