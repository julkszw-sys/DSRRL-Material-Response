#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::resource_bridges {

enum class equipment_legacy_spec_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    fail_spec_rgb_base,
    fail_invalid_dxbc,
    fail_color0,
    fail_spec_pow,
    fail_angular,
    fail_t9_split_sum,
    fail_rebuild,
    fail_postcondition
};

// Source must already be the exact diffuse-v1 MR receiver for a Phn Spc
// HemEnv/HemEnvLerp host. This materializer adds a split PTDE SpecRGB t10
// sample (t1 alpha/roughness preserved), multiplies raw PTDE c101, removes
// the DSR-only SPEC pow(2.2), angular/horizon attenuation and t9 split-sum,
// then applies semantic COLOR0 before the existing downstream composition.
// EnvSpec source selection, native DSR t12/t14 resources and dynamic LOD are
// intentionally preserved by this diagnostic.
equipment_legacy_spec_result
materialize_equipment_legacy_spec_response(
    const std::uint8_t *source,
    std::size_t size,
    std::vector<std::uint8_t> &output) noexcept;

} // namespace dsrrl::operators::resource_bridges
