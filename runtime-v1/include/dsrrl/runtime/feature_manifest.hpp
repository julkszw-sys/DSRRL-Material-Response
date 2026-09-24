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

struct runtime_feature_entry {
    core::operator_id op = core::operator_id::material_response;
    runtime_feature_stage stage = runtime_feature_stage::rejected;
    bool default_enabled = false;
    const char *name = "";
};

inline constexpr std::array<runtime_feature_entry, core::operator_count>
k_runtime_feature_manifest = {{
    {core::operator_id::material_response,
     runtime_feature_stage::current_wired,true,"material_response"},
    {core::operator_id::upper_lower,
     runtime_feature_stage::current_wired,true,"upper_lower"},
    {core::operator_id::hemdir3,
     runtime_feature_stage::future_partial,false,"hemdir3"},
    {core::operator_id::spec_rgb,
     runtime_feature_stage::current_wired,true,"spec_rgb"},
    {core::operator_id::env_spec,
     runtime_feature_stage::future_partial,false,"env_spec"},
    {core::operator_id::envspec_nospc_delete,
     runtime_feature_stage::current_wired,true,"envspec_nospc_delete"},
    {core::operator_id::envspec_pmetal_diagnostic,
     runtime_feature_stage::diagnostic_only,false,"envspec_pmetal_diagnostic"},
    {core::operator_id::env_diffuse,
     runtime_feature_stage::future_partial,false,"env_diffuse"},
    {core::operator_id::point_light,
     runtime_feature_stage::future_partial,false,"point_light"},
    {core::operator_id::pointlight_pnts_attenuation,
     runtime_feature_stage::current_wired,true,"pointlight_pnts_attenuation"},
    {core::operator_id::local_specular_legacy,
     runtime_feature_stage::future_partial,false,"local_specular_legacy"},
    {core::operator_id::subsurface,
     runtime_feature_stage::future_candidate,false,"subsurface"},
    {core::operator_id::diffuse,
     runtime_feature_stage::current_wired,true,"diffuse"},
    {core::operator_id::normal,
     runtime_feature_stage::current_wired,true,"normal"},
    {core::operator_id::diffuse_material_domain,
     runtime_feature_stage::current_wired,true,"diffuse_material_domain"},
    {core::operator_id::terminal_sat_rgb,
     runtime_feature_stage::current_wired,true,"terminal_sat_rgb"},
    {core::operator_id::terminal_sat_rgba,
     runtime_feature_stage::rejected,false,"terminal_sat_rgba"},
    {core::operator_id::fixed_postfog_identity,
     runtime_feature_stage::current_wired,true,"fixed_postfog_identity"},
    {core::operator_id::faceeye_shadow_legacy,
     runtime_feature_stage::future_partial,false,"faceeye_shadow_legacy"},
    {core::operator_id::post_bloom,
     runtime_feature_stage::blocked_preflight,false,"post_bloom"},
    {core::operator_id::post_hdr,
     runtime_feature_stage::blocked_preflight,false,"post_hdr"},
    {core::operator_id::dsr_native_sfx,
     runtime_feature_stage::host_preserve,false,"dsr_native_sfx"},
    {core::operator_id::dsr_sfx_inverse_tonemap,
     runtime_feature_stage::host_preserve,false,"dsr_sfx_inverse_tonemap"}
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

constexpr std::size_t runtime_default_enabled_count() noexcept
{
    std::size_t count = 0;
    for (const auto &entry : k_runtime_feature_manifest)
        if (entry.default_enabled)
            ++count;
    return count;
}

static_assert(
    runtime_default_enabled_count() == 10u,
    "A7 must not silently change the ten A1+A3 default-enabled runtime islands.");

} // namespace dsrrl::runtime
