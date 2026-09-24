#pragma once

#include "dsrrl/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

enum class runtime_feature_stage : std::uint8_t {
    current_wired = 0,
    future_candidate,
    future_partial,
    blocked_preflight,
    diagnostic_only,
    host_preserve,
    rejected
};

enum class runtime_boot_policy : std::uint8_t {
    hold_off = 0,
    enable_immediately,
    runtime_preflight
};

struct runtime_feature_entry {
    core::operator_id op = core::operator_id::material_response;
    runtime_feature_stage stage = runtime_feature_stage::rejected;
    runtime_boot_policy boot_policy = runtime_boot_policy::hold_off;
    const char *name = "";
};

inline constexpr std::array<runtime_feature_entry, core::operator_count>
k_runtime_feature_manifest = {{
    {core::operator_id::material_response,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"material_response"},
    {core::operator_id::upper_lower,
     runtime_feature_stage::current_wired,runtime_boot_policy::runtime_preflight,"upper_lower"},
    {core::operator_id::hemdir3,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"hemdir3"},
    {core::operator_id::spec_rgb,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"spec_rgb"},
    {core::operator_id::env_spec,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"env_spec"},
    {core::operator_id::envspec_nospc_delete,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"envspec_nospc_delete"},
    {core::operator_id::envspec_pmetal_diagnostic,
     runtime_feature_stage::diagnostic_only,runtime_boot_policy::hold_off,"envspec_pmetal_diagnostic"},
    {core::operator_id::env_diffuse,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"env_diffuse"},
    {core::operator_id::point_light,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"point_light"},
    {core::operator_id::pointlight_pnts_attenuation,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"pointlight_pnts_attenuation"},
    {core::operator_id::local_specular_legacy,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"local_specular_legacy"},
    {core::operator_id::subsurface,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"subsurface"},
    {core::operator_id::diffuse,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"diffuse"},
    {core::operator_id::normal,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"normal"},
    {core::operator_id::diffuse_material_domain,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"diffuse_material_domain"},
    {core::operator_id::terminal_sat_rgb,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"terminal_sat_rgb"},
    {core::operator_id::terminal_sat_rgba,
     runtime_feature_stage::rejected,runtime_boot_policy::hold_off,"terminal_sat_rgba"},
    {core::operator_id::fixed_postfog_identity,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"fixed_postfog_identity"},
    {core::operator_id::faceeye_shadow_legacy,
     runtime_feature_stage::future_partial,runtime_boot_policy::hold_off,"faceeye_shadow_legacy"},
    {core::operator_id::post_bloom,
     runtime_feature_stage::blocked_preflight,runtime_boot_policy::hold_off,"post_bloom"},
    {core::operator_id::post_hdr,
     runtime_feature_stage::blocked_preflight,runtime_boot_policy::hold_off,"post_hdr"},
    {core::operator_id::dsr_native_sfx,
     runtime_feature_stage::host_preserve,runtime_boot_policy::hold_off,"dsr_native_sfx"},
    {core::operator_id::dsr_sfx_inverse_tonemap,
     runtime_feature_stage::host_preserve,runtime_boot_policy::hold_off,"dsr_sfx_inverse_tonemap"},
    {core::operator_id::pmetal_black_safe_source,
     runtime_feature_stage::current_wired,runtime_boot_policy::runtime_preflight,"pmetal_black_safe_source"},
    {core::operator_id::pmetal_black_safe_v10,
     runtime_feature_stage::current_wired,runtime_boot_policy::enable_immediately,"pmetal_black_safe_v10"}
}};

constexpr bool runtime_feature_manifest_is_ordered_complete() noexcept
{
    if (k_runtime_feature_manifest.size() != core::operator_count)
        return false;

    for (std::size_t i = 0; i < k_runtime_feature_manifest.size(); ++i)
        if (static_cast<std::size_t>(k_runtime_feature_manifest[i].op) != i)
            return false;

    return true;
}

static_assert(
    runtime_feature_manifest_is_ordered_complete(),
    "Every Renderer Core operator must have exactly one ordered runtime feature entry.");

constexpr const runtime_feature_entry &runtime_feature_entry_for(
    core::operator_id op) noexcept
{
    return k_runtime_feature_manifest[static_cast<std::size_t>(op)];
}

constexpr bool runtime_feature_is_currently_wired(
    core::operator_id op) noexcept
{
    return runtime_feature_entry_for(op).stage ==
        runtime_feature_stage::current_wired;
}

constexpr bool runtime_feature_requires_detailed_readiness(
    core::operator_id op) noexcept
{
    const auto stage = runtime_feature_entry_for(op).stage;
    return stage == runtime_feature_stage::future_candidate ||
           stage == runtime_feature_stage::future_partial;
}

constexpr bool runtime_feature_is_hard_blocked(
    core::operator_id op) noexcept
{
    const auto stage = runtime_feature_entry_for(op).stage;
    return stage == runtime_feature_stage::blocked_preflight ||
           stage == runtime_feature_stage::diagnostic_only ||
           stage == runtime_feature_stage::host_preserve ||
           stage == runtime_feature_stage::rejected;
}

constexpr bool runtime_feature_boot_enabled(
    core::operator_id op) noexcept
{
    return runtime_feature_entry_for(op).boot_policy ==
        runtime_boot_policy::enable_immediately;
}

constexpr bool runtime_feature_needs_boot_preflight(
    core::operator_id op) noexcept
{
    return runtime_feature_entry_for(op).boot_policy ==
        runtime_boot_policy::runtime_preflight;
}

constexpr std::size_t runtime_boot_enabled_count() noexcept
{
    std::size_t count = 0;
    for (const auto &entry : k_runtime_feature_manifest)
        if (entry.boot_policy == runtime_boot_policy::enable_immediately)
            ++count;
    return count;
}

constexpr std::size_t runtime_boot_preflight_count() noexcept
{
    std::size_t count = 0;
    for (const auto &entry : k_runtime_feature_manifest)
        if (entry.boot_policy == runtime_boot_policy::runtime_preflight)
            ++count;
    return count;
}

static_assert(
    runtime_boot_enabled_count() == 11u,
    "Eleven islands are armed; P_Metal V10 remains exact-material/exact-receiver gated.");
static_assert(
    runtime_boot_preflight_count() == 2u,
    "U/L and exact P_Metal black-safe source are the two runtime-preflight islands.");
static_assert(
    runtime_feature_needs_boot_preflight(core::operator_id::upper_lower),
    "Upper/Lower must remain fail-open OFF until its exact producer hooks pass.");
static_assert(
    runtime_feature_needs_boot_preflight(core::operator_id::pmetal_black_safe_source),
    "P_Metal black-safe source must remain fail-open OFF until the V13 producer guard passes.");

} // namespace dsrrl::runtime
