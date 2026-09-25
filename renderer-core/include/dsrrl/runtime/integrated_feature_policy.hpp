#pragma once

#include "dsrrl/core/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::runtime {

enum class integrated_stage : std::uint8_t {
    wired = 0,
    deferred,
    diagnostic_only,
    blocked,
    host_preserve,
    rejected
};

enum class integrated_boot_policy : std::uint8_t {
    hold_off = 0,
    enable_immediately
};

struct integrated_feature_entry {
    core::operator_id op = core::operator_id::material_response;
    integrated_stage stage = integrated_stage::deferred;
    integrated_boot_policy boot_policy = integrated_boot_policy::hold_off;
    const char *name = "";
};

// Canonical integrated construction policy.
//
// Only islands with source-complete glue and an operator-local fail-open
// contract are boot-armed. Presence of predecessor code is not sufficient.
// In particular U/L, legacy EnvSpec, full PointLight/local-specular,
// EnvDiffuse, HemDir3, FaceEye and both P_Metal source islands remain OFF
// until their current canonical readiness/solver gates are independently
// closed.
inline constexpr std::array<integrated_feature_entry, core::operator_count>
k_integrated_feature_policy = {{
    {core::operator_id::material_response,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "material_response"},
    {core::operator_id::upper_lower,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "upper_lower"},
    {core::operator_id::hemdir3,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "hemdir3"},
    {core::operator_id::spec_rgb,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "spec_rgb"},
    {core::operator_id::env_spec,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "env_spec"},
    {core::operator_id::envspec_nospc_delete,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "envspec_nospc_delete"},
    {core::operator_id::envspec_pmetal_diagnostic,
     integrated_stage::diagnostic_only, integrated_boot_policy::hold_off,
     "envspec_pmetal_diagnostic"},
    {core::operator_id::env_diffuse,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "env_diffuse"},
    {core::operator_id::point_light,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "point_light"},
    {core::operator_id::pointlight_pnts_attenuation,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "pointlight_pnts_attenuation"},
    {core::operator_id::local_specular_legacy,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "local_specular_legacy"},
    {core::operator_id::subsurface,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "subsurface"},
    {core::operator_id::diffuse,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "diffuse"},
    {core::operator_id::normal,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "normal"},
    {core::operator_id::diffuse_material_domain,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "diffuse_material_domain"},
    {core::operator_id::terminal_sat_rgb,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "terminal_sat_rgb"},
    {core::operator_id::terminal_sat_rgba,
     integrated_stage::rejected, integrated_boot_policy::hold_off,
     "terminal_sat_rgba"},
    {core::operator_id::fixed_postfog_identity,
     integrated_stage::wired, integrated_boot_policy::enable_immediately,
     "fixed_postfog_identity"},
    {core::operator_id::faceeye_shadow_legacy,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "faceeye_shadow_legacy"},
    {core::operator_id::post_bloom,
     integrated_stage::blocked, integrated_boot_policy::hold_off,
     "post_bloom"},
    {core::operator_id::post_hdr,
     integrated_stage::blocked, integrated_boot_policy::hold_off,
     "post_hdr"},
    {core::operator_id::dsr_native_sfx,
     integrated_stage::host_preserve, integrated_boot_policy::hold_off,
     "dsr_native_sfx"},
    {core::operator_id::dsr_sfx_inverse_tonemap,
     integrated_stage::host_preserve, integrated_boot_policy::hold_off,
     "dsr_sfx_inverse_tonemap"},
    {core::operator_id::pmetal_black_safe_source,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "pmetal_black_safe_source"},
    {core::operator_id::pmetal_black_safe_v10,
     integrated_stage::deferred, integrated_boot_policy::hold_off,
     "pmetal_black_safe_v10"}
}};

constexpr bool integrated_feature_policy_is_ordered_complete() noexcept
{
    if (k_integrated_feature_policy.size() != core::operator_count)
        return false;

    for (std::size_t i = 0; i < k_integrated_feature_policy.size(); ++i)
        if (static_cast<std::size_t>(k_integrated_feature_policy[i].op) != i)
            return false;

    return true;
}

constexpr const integrated_feature_entry &integrated_feature_entry_for(
    core::operator_id op) noexcept
{
    return k_integrated_feature_policy[static_cast<std::size_t>(op)];
}

constexpr bool integrated_boot_enabled(core::operator_id op) noexcept
{
    return integrated_feature_entry_for(op).boot_policy ==
        integrated_boot_policy::enable_immediately;
}

constexpr std::size_t integrated_boot_enabled_count() noexcept
{
    std::size_t count = 0;
    for (const auto &entry : k_integrated_feature_policy)
        if (entry.boot_policy == integrated_boot_policy::enable_immediately)
            ++count;
    return count;
}

static_assert(
    integrated_feature_policy_is_ordered_complete(),
    "Every Renderer Core operator must have exactly one integrated policy entry.");

static_assert(
    integrated_boot_enabled_count() == 10u,
    "Integrated A2 construction intentionally boots exactly ten closed/wired islands.");

static_assert(!integrated_boot_enabled(core::operator_id::upper_lower));
static_assert(!integrated_boot_enabled(core::operator_id::env_spec));
static_assert(!integrated_boot_enabled(core::operator_id::point_light));
static_assert(!integrated_boot_enabled(core::operator_id::local_specular_legacy));
static_assert(!integrated_boot_enabled(core::operator_id::pmetal_black_safe_source));
static_assert(!integrated_boot_enabled(core::operator_id::pmetal_black_safe_v10));

} // namespace dsrrl::runtime
