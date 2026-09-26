#pragma once

#include "dsrrl/core/types.hpp"
#include "dsrrl/core/carrier_abi.hpp"

#include <array>
#include <cstdint>

namespace dsrrl::core {

enum class draw_transaction_mode : std::uint8_t {
    not_applicable = 0,
    create_time_safe,
    draw_required,
    blocked,
    host_preserve
};

enum draw_mutation_bit : std::uint32_t {
    draw_mutation_none = 0,
    draw_mutation_shader = 1u << 0,
    draw_mutation_constant_buffer = 1u << 1,
    draw_mutation_srv = 1u << 2,
    draw_mutation_sampler = 1u << 3
};

inline constexpr std::uint32_t shared_draw_executor_mutation_mask =
    draw_mutation_shader |
    draw_mutation_constant_buffer |
    draw_mutation_srv |
    draw_mutation_sampler;

struct draw_transaction_policy {
    operator_id op = operator_id::material_response;
    draw_transaction_mode mode =
        draw_transaction_mode::not_applicable;
    std::uint32_t required_mutation_mask =
        draw_mutation_none;
    std::uint32_t allowed_mutation_mask =
        draw_mutation_none;
    bool exact_receiver_gate = false;
    bool exact_material_gate = false;
    bool full_restore_required = false;
};

inline constexpr std::array<
    draw_transaction_policy,
    operator_count> k_draw_transaction_policies = {{
    {operator_id::material_response, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer, true, true, true},
    {operator_id::upper_lower, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer,
     true, false, true},
    {operator_id::hemdir3, draw_transaction_mode::draw_required,
     draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer, true, false, true},
    {operator_id::spec_rgb, draw_transaction_mode::draw_required,
     draw_mutation_srv,
     draw_mutation_shader | draw_mutation_srv,
     true, true, true},
    {operator_id::env_spec, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     true, true, true},
    {operator_id::envspec_nospc_delete, draw_transaction_mode::create_time_safe,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::envspec_pmetal_diagnostic, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     true, true, true},
    {operator_id::env_diffuse, draw_transaction_mode::draw_required,
     draw_mutation_constant_buffer | draw_mutation_srv |
         draw_mutation_sampler,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     true, false, true},
    {operator_id::point_light, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer, true, false, true},
    {operator_id::pointlight_pnts_attenuation,
     draw_transaction_mode::create_time_safe,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::local_specular_legacy, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer, true, true, true},
    {operator_id::subsurface, draw_transaction_mode::draw_required,
     draw_mutation_shader,
     draw_mutation_shader, true, true, true},
    {operator_id::diffuse, draw_transaction_mode::draw_required,
     draw_mutation_srv,
     draw_mutation_srv, true, true, true},
    {operator_id::normal, draw_transaction_mode::draw_required,
     draw_mutation_srv, draw_mutation_srv, true, false, true},
    {operator_id::diffuse_material_domain,
     draw_transaction_mode::create_time_safe,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::terminal_sat_rgb, draw_transaction_mode::create_time_safe,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::terminal_sat_rgba, draw_transaction_mode::blocked,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::fixed_postfog_identity,
     draw_transaction_mode::create_time_safe,
     draw_mutation_none, draw_mutation_shader, true, false, false},
    {operator_id::faceeye_shadow_legacy, draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_srv | draw_mutation_sampler,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     true, false, true},
    {operator_id::post_bloom, draw_transaction_mode::blocked,
     draw_mutation_none, draw_mutation_none, false, false, false},
    {operator_id::post_hdr, draw_transaction_mode::blocked,
     draw_mutation_none, draw_mutation_none, false, false, false},
    {operator_id::dsr_native_sfx, draw_transaction_mode::host_preserve,
     draw_mutation_none, draw_mutation_none, false, false, false},
    {operator_id::dsr_sfx_inverse_tonemap,
     draw_transaction_mode::host_preserve,
     draw_mutation_none, draw_mutation_none, false, false, false},
    {operator_id::pmetal_black_safe_source,
     draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     draw_mutation_shader | draw_mutation_constant_buffer |
         draw_mutation_srv | draw_mutation_sampler,
     true, true, true},
    {operator_id::pmetal_black_safe_v10,
     draw_transaction_mode::draw_required,
     draw_mutation_shader | draw_mutation_constant_buffer,
     draw_mutation_shader | draw_mutation_constant_buffer, true, true, true},
}};

constexpr const draw_transaction_policy &draw_policy(
    operator_id op) noexcept
{
    return k_draw_transaction_policies[
        static_cast<std::size_t>(op)];
}

constexpr bool requires_draw_transaction(
    operator_id op) noexcept
{
    return draw_policy(op).mode ==
        draw_transaction_mode::draw_required;
}

constexpr bool create_time_safe_operator(
    operator_id op) noexcept
{
    return draw_policy(op).mode ==
        draw_transaction_mode::create_time_safe;
}

constexpr std::uint32_t draw_policy_carrier_write_mask(
    operator_id op) noexcept
{
    switch (op) {
    case operator_id::upper_lower:
        return carrier_ul_mask;
    case operator_id::hemdir3:
        return carrier_hemdir3_mask;
    default:
        return 0u;
    }
}

constexpr bool draw_policy_table_valid() noexcept
{
    for (std::size_t i = 0;
         i < k_draw_transaction_policies.size();
         ++i) {
        const auto &policy =
            k_draw_transaction_policies[i];

        if (static_cast<std::size_t>(policy.op) != i)
            return false;

        if ((policy.required_mutation_mask &
             ~policy.allowed_mutation_mask) != 0u)
            return false;

        if (policy.mode ==
                draw_transaction_mode::draw_required &&
            (!policy.full_restore_required ||
             policy.required_mutation_mask ==
                 draw_mutation_none))
            return false;

        if (policy.mode ==
                draw_transaction_mode::draw_required &&
            (policy.allowed_mutation_mask &
             ~shared_draw_executor_mutation_mask) != 0u)
            return false;
    }

    return true;
}

static_assert(
    k_draw_transaction_policies.size() ==
        operator_count,
    "Every island must have an explicit draw transaction policy.");
static_assert(
    draw_policy_table_valid(),
    "Draw transaction policy table must be ordered and internally consistent.");

} // namespace dsrrl::core
