#pragma once

#include "dsrrl/operators/point_light/local_specular_microfacet_windows.hpp"
#include "dsrrl/operators/point_light/local_specular_receiver_registry.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::point_light {

enum class fixed_local_specular_plan_result : std::uint8_t {
    ready = 0,
    pass_not_local_specular_receiver,
    pass_clustered_membership_not_owned,
    fail_microfacet_window_scan,
    fail_fixed_light_count
};

struct fixed_local_specular_light_plan {
    local_specular_microfacet_window microfacet_window{};
    std::uint8_t raw_q_t19_index = 0u;
    std::uint16_t position_begin_cb = 0u;
    std::uint16_t color_end_cb = 0u;
};

struct fixed_local_specular_patch_plan {
    fixed_local_specular_plan_result result =
        fixed_local_specular_plan_result::
            pass_not_local_specular_receiver;
    local_specular_receiver_identity identity{};
    std::array<fixed_local_specular_light_plan,4> lights{};
    std::uint8_t light_count = 0u;

    // Canonical draw-specific carriers for the eventual replacement shader.
    std::uint8_t material_cb_slot = 12u;
    std::uint8_t raw_q_srv_slot = 19u;

    // Explicit anti-hybrid contract.
    bool replace_complete_microfacet_window = false;
    bool bypass_stock_roughness_tail = false;
    bool bypass_stock_common_ndotl_specular = false;
    bool preserve_stock_diffuse = true;
};

// Source-complete planning seam for fixed PntSS/PntSSSS only.
//
// It combines:
//   exact original DXBC SHA/size identity,
//   token-level 1/2/4 microfacet-window attestation,
//   fixed native light-slot ABI,
//   canonical t19[i] raw-q and b12 material carriers.
//
// Clustered PntS deliberately returns
// pass_clustered_membership_not_owned until the independent PTDE-selected
// four-light membership sidecar is integrated. This function performs no
// visible shader mutation.
// Secondary deterministic seam for unit tests and offline tooling after
// exact receiver identity + window attestation have already been established.
// Production code should call build_fixed_local_specular_patch_plan().
fixed_local_specular_patch_plan
build_fixed_local_specular_patch_plan_from_attested(
    const local_specular_receiver_identity &identity,
    const local_specular_microfacet_window_scan &scan) noexcept;

fixed_local_specular_patch_plan
build_fixed_local_specular_patch_plan(
    const void *pixel_shader_code,
    std::size_t code_size) noexcept;

} // namespace dsrrl::operators::point_light
