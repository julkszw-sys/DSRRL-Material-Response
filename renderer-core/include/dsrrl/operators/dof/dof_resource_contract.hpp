#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::dof {

enum class ptde_surface_role : std::uint8_t {
    full_prefix = 0,
    full_terminal,
    half_rate,
    half_rate_result,
    half_blur,
    quarter_ping,
    quarter_pong,
    count
};

struct ptde_surface_pair_desc {
    ptde_surface_role role = ptde_surface_role::full_prefix;
    std::uint16_t target_offset = 0u;
    std::uint16_t srv_offset = 0u;
    legacy_raster_desc raster{};
};

// Exact resource subset referenced by PTDE ImageProcessDof_Flat ctor
// 0x00FFF170. ImageProcessGlobals owns additional half/quarter pairs, but
// +B4/+C4 and +E0/+EC are not referenced by this DoF graph and therefore
// are intentionally excluded from the private island (narrowest carrier).
inline constexpr std::array<ptde_surface_pair_desc, 7>
ptde_fixed_surface_pairs = {{
    {ptde_surface_role::full_prefix,   0x005Cu, 0x0060u,
        {1024u, 720u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::full_terminal, 0x0080u, 0x0084u,
        {1024u, 720u, legacy_format::a8r8g8b8}},

    {ptde_surface_role::half_rate,        0x00A8u, 0x00B8u,
        {512u, 360u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::half_rate_result, 0x00ACu, 0x00BCu,
        {512u, 360u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::half_blur,        0x00B0u, 0x00C0u,
        {512u, 360u, legacy_format::a8r8g8b8}},

    {ptde_surface_role::quarter_ping, 0x00D8u, 0x00E4u,
        {256u, 180u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::quarter_pong, 0x00DCu, 0x00E8u,
        {256u, 180u, legacy_format::a8r8g8b8}}
}};

constexpr const ptde_surface_pair_desc *find_ptde_surface(
    ptde_surface_role role) noexcept
{
    for (const auto &entry : ptde_fixed_surface_pairs)
        if (entry.role == role)
            return &entry;
    return nullptr;
}

constexpr std::size_t ptde_surface_count(
    std::uint16_t width,
    std::uint16_t height) noexcept
{
    std::size_t count = 0u;
    for (const auto &entry : ptde_fixed_surface_pairs)
        if (entry.raster.width == width &&
            entry.raster.height == height)
            ++count;
    return count;
}

constexpr bool ptde_fixed_surface_set_is_exact() noexcept
{
    if (ptde_fixed_surface_pairs.size() !=
        static_cast<std::size_t>(ptde_surface_role::count))
        return false;

    if (ptde_surface_count(1024u, 720u) != 2u ||
        ptde_surface_count(512u, 360u) != 3u ||
        ptde_surface_count(256u, 180u) != 2u)
        return false;

    for (const auto &entry : ptde_fixed_surface_pairs) {
        if (entry.target_offset == 0u ||
            entry.srv_offset == 0u ||
            entry.raster.format != legacy_format::a8r8g8b8)
            return false;
    }

    const auto *terminal =
        find_ptde_surface(ptde_surface_role::full_terminal);
    return terminal != nullptr &&
        terminal->target_offset == 0x0080u &&
        terminal->srv_offset == 0x0084u &&
        terminal->raster.width == 1024u &&
        terminal->raster.height == 720u;
}

// Raw builder resource arguments recovered from PTDE ctor 0x00FFF170.
// These preserve the constructor ABI instead of guessing semantic slots:
// target = builder arg3, pass = arg5, primary = arg6, additional inputs
// are args7..10. nonzero arg11 is retained as a pass flag.
struct ptde_pass_resource_contract {
    std::uint8_t pass = 0u;
    std::uint16_t target_offset = 0u;
    std::uint16_t arg6_source = 0u;
    std::uint16_t arg7_source = 0u;
    std::uint16_t arg8_source = 0u;
    std::uint16_t arg9_source = 0u;
    std::uint16_t arg10_source = 0u;
    std::uint8_t arg11_flag = 0u;
};

inline constexpr std::array<ptde_pass_resource_contract, 9>
ptde_exact_pass_resources = {{
    {0x00u, 0x005Cu, 0x0068u, 0u,      0u,      0u,      0u,      1u},
    {0x02u, 0x00A8u, 0x0060u, 0u,      0u,      0u,      0u,      1u},
    {0x0Du, 0x00ACu, 0x00B8u, 0u,      0u,      0u,      0u,      1u},
    {0x01u, 0x00DCu, 0x00B8u, 0u,      0u,      0u,      0u,      1u},
    {0x0Du, 0x00D8u, 0x00E8u, 0u,      0u,      0u,      0u,      1u},
    {0x0Fu, 0x00B0u, 0x00E4u, 0u,      0u,      0u,      0u,      1u},
    {0x03u, 0x00DCu, 0u,      0x0068u, 0u,      0u,      0u,      1u},
    {0x0Eu, 0x00D8u, 0x00E8u, 0u,      0u,      0u,      0u,      1u},
    {0x10u, 0x0080u, 0x0060u, 0x00B8u, 0x00BCu, 0x00C0u, 0x00E4u, 1u}
}};

constexpr bool ptde_exact_pass_resource_graph_is_structurally_closed() noexcept
{
    if (ptde_exact_pass_resources.size() != ptde_flat_passes.size())
        return false;

    for (std::size_t i = 0u; i < ptde_flat_passes.size(); ++i)
        if (ptde_exact_pass_resources[i].pass != ptde_flat_passes[i])
            return false;

    const auto &terminal = ptde_exact_pass_resources.back();
    return
        terminal.pass == 0x10u &&
        terminal.target_offset == 0x0080u &&
        terminal.arg6_source == 0x0060u &&
        terminal.arg7_source == 0x00B8u &&
        terminal.arg8_source == 0x00BCu &&
        terminal.arg9_source == 0x00C0u &&
        terminal.arg10_source == 0x00E4u &&
        terminal.arg11_flag == 1u;
}

enum class cheap_source_kind : std::uint8_t {
    none = 0,
    scene,
    depth,
    half_rate,
    half_rate_result,
    half_blur,
    quarter_ping,
    quarter_pong
};

struct cheap_half_seed_pass_contract {
    std::uint8_t logical_pass = 0u;
    retained_shader_role shader = retained_shader_role::count;
    ptde_surface_role target = ptde_surface_role::half_rate;
    std::array<cheap_source_kind, 6> sources{};
    std::uint8_t rt0_write_mask = 0x0Fu;
    bool inherit_host_om = false;
};

inline constexpr std::array<cheap_half_seed_pass_contract, 9>
cheap_half_seed_passes = {{
    {0x00u, retained_shader_role::dof_rate_plain,
        ptde_surface_role::half_rate,
        {cheap_source_kind::none, cheap_source_kind::depth,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x08u, false},
    {0x02u, retained_shader_role::downsample,
        ptde_surface_role::half_rate,
        {cheap_source_kind::scene, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x07u, false},
    {0x0Du, retained_shader_role::unfocus_3x3,
        ptde_surface_role::half_rate_result,
        {cheap_source_kind::half_rate, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x0Fu, false},
    {0x01u, retained_shader_role::downsample,
        ptde_surface_role::quarter_pong,
        {cheap_source_kind::half_rate, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x0Fu, false},
    {0x0Du, retained_shader_role::unfocus_3x3,
        ptde_surface_role::quarter_ping,
        {cheap_source_kind::quarter_pong, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x0Fu, false},
    {0x0Fu, retained_shader_role::blur_upsample,
        ptde_surface_role::half_blur,
        {cheap_source_kind::quarter_ping, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x0Fu, false},
    {0x03u, retained_shader_role::near_rate,
        ptde_surface_role::quarter_pong,
        {cheap_source_kind::quarter_ping, cheap_source_kind::depth,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x08u, false},
    {0x0Eu, retained_shader_role::unfocus_near_rate_3x3,
        ptde_surface_role::quarter_ping,
        {cheap_source_kind::quarter_pong, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none,
         cheap_source_kind::none, cheap_source_kind::none},
        0x08u, false},
    {0x10u, retained_shader_role::dof_composite,
        ptde_surface_role::full_terminal,
        {cheap_source_kind::scene, cheap_source_kind::half_rate,
         cheap_source_kind::half_rate_result, cheap_source_kind::half_blur,
         cheap_source_kind::quarter_ping, cheap_source_kind::depth},
        0x0Fu, true}
}};

constexpr bool cheap_half_seed_graph_is_structurally_closed() noexcept
{
    if (cheap_half_seed_passes.size() != 9u)
        return false;

    if (cheap_half_seed_passes[0].target != ptde_surface_role::half_rate ||
        cheap_half_seed_passes[0].shader != retained_shader_role::dof_rate_plain ||
        cheap_half_seed_passes[0].rt0_write_mask != 0x08u ||
        cheap_half_seed_passes[1].target != ptde_surface_role::half_rate ||
        cheap_half_seed_passes[1].shader != retained_shader_role::downsample ||
        cheap_half_seed_passes[1].rt0_write_mask != 0x07u)
        return false;

    const auto &terminal = cheap_half_seed_passes.back();
    return
        terminal.logical_pass == 0x10u &&
        terminal.shader == retained_shader_role::dof_composite &&
        terminal.target == ptde_surface_role::full_terminal &&
        terminal.sources[0] == cheap_source_kind::scene &&
        terminal.sources[1] == cheap_source_kind::half_rate &&
        terminal.sources[2] == cheap_source_kind::half_rate_result &&
        terminal.sources[3] == cheap_source_kind::half_blur &&
        terminal.sources[4] == cheap_source_kind::quarter_ping &&
        terminal.sources[5] == cheap_source_kind::depth &&
        terminal.inherit_host_om;
}

struct plain_dofrate_switch_contract {
    std::uintptr_t instruction_rva = 0u;
    std::array<std::uint8_t, 5> expected_cb_selector{};
    std::array<std::uint8_t, 5> replacement_plain_selector{};
    std::uint32_t stock_runtime_shader_id = 0u;
    std::uint32_t ptde_bridge_runtime_shader_id = 0u;
};

// DSR ImageProcessDof_Flat pass 0x0D executor:
//   mov edx, 0xDF0   ; retained DofRate_CB
// Exact PTDE bridge uses retained plain DofRate (0xDEF), whose rate algebra
// is preserved while its t1 frontend is ordinary Texture2D like PTDE.
inline constexpr plain_dofrate_switch_contract
dsr_plain_dofrate_switch = {
    0x00456489u,
    {0xBAu, 0xF0u, 0x0Du, 0x00u, 0x00u},
    {0xBAu, 0xEFu, 0x0Du, 0x00u, 0x00u},
    dsr_dofrate_cb_runtime_shader_id,
    dsr_dofrate_plain_runtime_shader_id
};

template<std::size_t N>
constexpr bool byte_array_equal(
    const std::array<std::uint8_t, N> &a,
    const std::array<std::uint8_t, N> &b) noexcept
{
    for (std::size_t i = 0u; i < N; ++i)
        if (a[i] != b[i])
            return false;
    return true;
}

constexpr bool plain_dofrate_switch_is_exact() noexcept
{
    return
        dsr_plain_dofrate_switch.instruction_rva == 0x00456489u &&
        dsr_plain_dofrate_switch.stock_runtime_shader_id == 0x0DF0u &&
        dsr_plain_dofrate_switch.ptde_bridge_runtime_shader_id == 0x0DEFu &&
        byte_array_equal(
            dsr_plain_dofrate_switch.expected_cb_selector,
            std::array<std::uint8_t, 5>{
                0xBAu, 0xF0u, 0x0Du, 0x00u, 0x00u}) &&
        byte_array_equal(
            dsr_plain_dofrate_switch.replacement_plain_selector,
            std::array<std::uint8_t, 5>{
                0xBAu, 0xEFu, 0x0Du, 0x00u, 0x00u});
}

static_assert(ptde_fixed_surface_set_is_exact(),
    "PTDE DoF private set must remain 2x full + 3x half + 2x quarter BGRA8.");
static_assert(ptde_exact_pass_resource_graph_is_structurally_closed(),
    "PTDE DoF ctor resource graph drifted.");
static_assert(cheap_half_seed_graph_is_structurally_closed(),
    "Cheap DoF half-seed graph drifted.");
static_assert(plain_dofrate_switch_is_exact(),
    "DSR DoF plain-rate switch contract drifted.");

} // namespace dsrrl::operators::dof
