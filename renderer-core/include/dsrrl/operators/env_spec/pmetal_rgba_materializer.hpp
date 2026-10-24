#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dsrrl::operators::env_spec {

enum class pmetal_rgba_materialize_result : std::uint8_t {
    applied = 0,
    pass_not_candidate,
    pass_unknown_exact_sha,
    fail_invalid_dxbc,
    fail_diffuse_base,
    fail_a1_composition,
    fail_build131_precondition,
    fail_build131_rdef,
    fail_b12_rdef,
    fail_upper_lower_composition,
    fail_spec_rgb_consumer,
    fail_postcondition,
    fail_rebuild
};

struct pmetal_rgba_materialize_outcome {
    pmetal_rgba_materialize_result result =
        pmetal_rgba_materialize_result::pass_not_candidate;
    std::uint32_t receiver_id = 0;
    core::operator_mask composed_owners = 0;
    bool upper_lower_composed = false;
    // R6: exact PHN k135->SAT consumer is present. This flag says nothing
    // about producer validity; an unresolved producer uses unity fail-open.
    bool phn_scene_encoding_composed = false;
    // R7: exact PTDE Csd/Sdw shadow visibility producer is present on
    // stable receivers 33/34. Plain receiver 35 has no such branch.
    bool shadow_visibility_kernel_composed = false;
    bool spec_rgb_consumer = false;
    // Diagnostic-only: stable HemEnv t11 consumes the exact pre-draw-gain
    // EnvDiffuse endpoint carried in b12[3].xyz instead of DSR cb0[3]*cb0[79].x.
    bool envdiffuse_linear_consumer_diag = false;
};

// Independent original PTDE EnvDiffuse, exact stable RX33..35.
// Stock EnvSpec, t12/t14 and all other material operators remain untouched.
bool materialize_ptde_envdiffuse_stable_only(
    const std::uint8_t *stock, std::size_t n,
    std::vector<std::uint8_t> &out,
    std::uint32_t &receiver_id) noexcept;

pmetal_rgba_materialize_outcome
materialize_pmetal_rgba_receiver(
    const core::feature_registry &features,
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    bool compose_upper_lower,
    std::vector<std::uint8_t> &output) noexcept;

#if defined(DSRRL_V230_INDEPENDENT_ENVDIFFUSE)
// Create-time exact stock-HemEnv shader island. Changes ONLY the t11
// EnvDiffuse consumer (RGBA filter -> RGB/A -> independently supplied
// pre-draw-gain endpoint). EnvSpec t12 and its entire DSR shader body,
// SpecRGB, MR, common merge and other unrelated operators stay stock.
// Unknown stock PS identity/ABI fails open; no guessed shader family.
bool materialize_independent_envdiffuse_stable(
    const std::uint8_t *stock_source,
    std::size_t stock_size,
    std::uint32_t &receiver_id,
    std::vector<std::uint8_t> &output) noexcept;
#endif

} // namespace dsrrl::operators::env_spec
