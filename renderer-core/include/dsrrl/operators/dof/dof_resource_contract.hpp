#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::dof {

enum class ptde_surface_role : std::uint8_t {
    full_prefix = 0,
    full_terminal,
    half_a,
    half_b,
    half_c,
    half_d,
    quarter_a,
    quarter_b,
    quarter_c,
    count
};

struct ptde_surface_pair_desc {
    ptde_surface_role role = ptde_surface_role::full_prefix;
    std::uint16_t target_offset = 0u;
    std::uint16_t srv_offset = 0u;
    legacy_raster_desc raster{};
};

// Exact PTDE ImageProcessGlobals fixed resources used by the DoF family.
// target_offset is the TextureSurface/RT side; srv_offset is the paired
// RenderTexture/sample side. One D3D11 texture can supply both RTV and SRV
// in the bridge, but the two legacy identities remain explicit here.
inline constexpr std::array<ptde_surface_pair_desc, 9>
ptde_fixed_surface_pairs = {{
    {ptde_surface_role::full_prefix,   0x005Cu, 0x0060u,
        {1024u, 720u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::full_terminal, 0x0080u, 0x0084u,
        {1024u, 720u, legacy_format::a8r8g8b8}},

    {ptde_surface_role::half_a, 0x00A8u, 0x00B8u,
        {512u, 360u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::half_b, 0x00ACu, 0x00BCu,
        {512u, 360u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::half_c, 0x00B0u, 0x00C0u,
        {512u, 360u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::half_d, 0x00B4u, 0x00C4u,
        {512u, 360u, legacy_format::a8r8g8b8}},

    {ptde_surface_role::quarter_a, 0x00D8u, 0x00E4u,
        {256u, 180u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::quarter_b, 0x00DCu, 0x00E8u,
        {256u, 180u, legacy_format::a8r8g8b8}},
    {ptde_surface_role::quarter_c, 0x00E0u, 0x00ECu,
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
        ptde_surface_count(512u, 360u) != 4u ||
        ptde_surface_count(256u, 180u) != 3u)
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

struct verified_ptde_pass_edge {
    std::uint8_t pass = 0u;
    std::uint16_t source_offset = 0u;
    std::uint16_t target_offset = 0u;
    std::uint16_t stored_srv_offset = 0u;
};

// Only edges already closed directly by PTDE constructor/builder RE are
// represented here. The full nine-pass scheduler remains a separate gate.
inline constexpr std::array<verified_ptde_pass_edge, 2>
ptde_verified_prefix_edges = {{
    {0x00u, 0x0068u, 0x005Cu, 0x0060u},
    {0x02u, 0x0060u, 0x00A8u, 0x00B8u}
}};

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

constexpr bool plain_dofrate_switch_is_exact() noexcept
{
    return
        dsr_plain_dofrate_switch.instruction_rva == 0x00456489u &&
        dsr_plain_dofrate_switch.stock_runtime_shader_id == 0x0DF0u &&
        dsr_plain_dofrate_switch.ptde_bridge_runtime_shader_id == 0x0DEFu &&
        dsr_plain_dofrate_switch.expected_cb_selector ==
            std::array<std::uint8_t, 5>{
                0xBAu, 0xF0u, 0x0Du, 0x00u, 0x00u} &&
        dsr_plain_dofrate_switch.replacement_plain_selector ==
            std::array<std::uint8_t, 5>{
                0xBAu, 0xEFu, 0x0Du, 0x00u, 0x00u};
}

static_assert(ptde_fixed_surface_set_is_exact(),
    "PTDE DoF fixed resource set must remain 2x full + 4x half + 3x quarter BGRA8.");
static_assert(plain_dofrate_switch_is_exact(),
    "DSR DoF plain-rate switch contract drifted.");

} // namespace dsrrl::operators::dof
