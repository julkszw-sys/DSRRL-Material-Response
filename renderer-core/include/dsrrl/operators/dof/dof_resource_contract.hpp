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

enum class production_source_kind : std::uint8_t {
    none = 0,
    scene,
    depth,
    full_prefix,
    half_rate,
    half_rate_result,
    half_blur,
    quarter_ping,
    quarter_pong
};

struct production_seed_pass_contract {
    std::uint8_t logical_pass = 0u;
    retained_shader_role shader = retained_shader_role::count;
    ptde_surface_role target = ptde_surface_role::half_rate;
    std::array<production_source_kind, 6> sources{};
    std::uint8_t rt0_write_mask = 0x0Fu;
    bool inherit_host_om = false;
};

inline constexpr std::array<production_seed_pass_contract, 10>
production_seed_passes = {{
    // Synthetic pre-pass: materialize only scene RGB into PTDE's fixed
    // 1024x720 BGRA8 first-stage carrier.
    {0xFFu, retained_shader_role::downsample,
        ptde_surface_role::full_prefix,
        {production_source_kind::scene, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x07u, false},

    // PTDE pass00 at PTDE raster: write DofRate into alpha only.
    {0x00u, retained_shader_role::dof_rate_plain,
        ptde_surface_role::full_prefix,
        {production_source_kind::none, production_source_kind::depth,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x08u, false},

    // PTDE pass02: first resolution reduction occurs only here.
    {0x02u, retained_shader_role::downsample,
        ptde_surface_role::half_rate,
        {production_source_kind::full_prefix, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x0Fu, false},

    {0x0Du, retained_shader_role::unfocus_3x3,
        ptde_surface_role::half_rate_result,
        {production_source_kind::half_rate, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x0Fu, false},
    {0x01u, retained_shader_role::downsample,
        ptde_surface_role::quarter_pong,
        {production_source_kind::half_rate, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x0Fu, false},
    {0x0Du, retained_shader_role::unfocus_3x3,
        ptde_surface_role::quarter_ping,
        {production_source_kind::quarter_pong, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x0Fu, false},
    {0x0Fu, retained_shader_role::blur_upsample,
        ptde_surface_role::half_blur,
        {production_source_kind::quarter_ping, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x0Fu, false},

    // Retained DSR NearRate consumes only depth at t1. RGB in quarter_pong
    // survives from pass01 because this pass commits alpha only.
    {0x03u, retained_shader_role::near_rate,
        ptde_surface_role::quarter_pong,
        {production_source_kind::none, production_source_kind::depth,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x08u, false},
    {0x0Eu, retained_shader_role::unfocus_near_rate_3x3,
        ptde_surface_role::quarter_ping,
        {production_source_kind::quarter_pong, production_source_kind::none,
         production_source_kind::none, production_source_kind::none,
         production_source_kind::none, production_source_kind::none},
        0x08u, false},

    // Retained DSR terminal computes full-resolution rate from native t5.
    {0x10u, retained_shader_role::dof_composite,
        ptde_surface_role::full_terminal,
        {production_source_kind::scene, production_source_kind::half_rate,
         production_source_kind::half_rate_result, production_source_kind::half_blur,
         production_source_kind::quarter_ping, production_source_kind::depth},
        0x0Fu, true}
}};

constexpr bool production_seed_graph_is_structurally_closed() noexcept
{
    if (production_seed_passes.size() != 10u)
        return false;

    const auto &scene_seed = production_seed_passes[0];
    const auto &rate_seed = production_seed_passes[1];
    const auto &first_downsample = production_seed_passes[2];

    if (scene_seed.target != ptde_surface_role::full_prefix ||
        scene_seed.shader != retained_shader_role::downsample ||
        scene_seed.sources[0] != production_source_kind::scene ||
        scene_seed.rt0_write_mask != 0x07u ||
        rate_seed.logical_pass != 0x00u ||
        rate_seed.target != ptde_surface_role::full_prefix ||
        rate_seed.shader != retained_shader_role::dof_rate_plain ||
        rate_seed.sources[1] != production_source_kind::depth ||
        rate_seed.rt0_write_mask != 0x08u ||
        first_downsample.logical_pass != 0x02u ||
        first_downsample.target != ptde_surface_role::half_rate ||
        first_downsample.sources[0] != production_source_kind::full_prefix)
        return false;

    const auto *prefix =
        find_ptde_surface(ptde_surface_role::full_prefix);
    if (prefix == nullptr ||
        prefix->raster.width != 1024u ||
        prefix->raster.height != 720u ||
        prefix->raster.format != legacy_format::a8r8g8b8)
        return false;

    const auto &terminal = production_seed_passes.back();
    return
        terminal.logical_pass == 0x10u &&
        terminal.shader == retained_shader_role::dof_composite &&
        terminal.target == ptde_surface_role::full_terminal &&
        terminal.sources[0] == production_source_kind::scene &&
        terminal.sources[1] == production_source_kind::half_rate &&
        terminal.sources[2] == production_source_kind::half_rate_result &&
        terminal.sources[3] == production_source_kind::half_blur &&
        terminal.sources[4] == production_source_kind::quarter_ping &&
        terminal.sources[5] == production_source_kind::depth &&
        terminal.inherit_host_om;
}

static_assert(ptde_fixed_surface_set_is_exact(),
    "PTDE DoF private set must remain 2x full + 3x half + 2x quarter BGRA8.");
static_assert(ptde_exact_pass_resource_graph_is_structurally_closed(),
    "PTDE DoF ctor resource graph drifted.");
static_assert(production_seed_graph_is_structurally_closed(),
    "Production DoF seed graph must preserve the PTDE 1024x720 first raster.");

} // namespace dsrrl::operators::dof
