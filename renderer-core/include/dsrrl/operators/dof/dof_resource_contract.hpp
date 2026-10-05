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
static_assert(plain_dofrate_switch_is_exact(),
    "DSR DoF plain-rate switch contract drifted.");

} // namespace dsrrl::operators::dof
