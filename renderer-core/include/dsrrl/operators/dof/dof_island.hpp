#pragma once

#include <array>
#include <cstdint>

namespace dsrrl::operators::dof {

enum class flat_mode : std::uint8_t {
    unknown = 0,
    primary,
    alternate
};

enum class bridge_reason : std::uint8_t {
    ready = 0,
    disabled,
    wrong_receiver,
    unknown_mode,
    incomplete_graph,
    missing_private_depth_sidecar,
    missing_plain_dofrate,
    missing_fixed_raster_chain,
    missing_final_composite_cut,
    temporal_state_write_forbidden,
    stock_depth_write_forbidden
};

enum class legacy_format : std::uint8_t {
    a8r8g8b8 = 0
};

struct legacy_raster_desc {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    legacy_format format = legacy_format::a8r8g8b8;
};

struct temporal_write_set {
    bool taa_history = false;
    bool velocity = false;
    bool stock_native_depth = false;
};

struct activation_context {
    bool enabled = false;
    bool exact_imageprocess_dof_flat = false;
    flat_mode mode = flat_mode::unknown;
    bool graph_complete = false;
    bool private_depth_sidecar_ready = false;
    bool retained_plain_dofrate_ready = false;
    bool fixed_raster_chain_ready = false;
    bool final_composite_cut_verified = false;
    temporal_write_set writes{};
};

struct activation_decision {
    bool active = false;
    bridge_reason reason = bridge_reason::disabled;
};

inline constexpr std::array<std::uint8_t, 9> ptde_flat_passes = {
    0x00u, 0x02u, 0x0Du, 0x01u, 0x0Du, 0x0Fu, 0x03u, 0x0Eu, 0x10u
};

inline constexpr std::array<std::uint8_t, 8> dsr_flat_passes = {
    0x01u, 0x0Du, 0x01u, 0x0Du, 0x0Fu, 0x03u, 0x0Eu, 0x10u
};

inline constexpr std::array<legacy_raster_desc, 3> ptde_fixed_raster_ladder = {{
    {1024u, 720u, legacy_format::a8r8g8b8},
    {512u, 360u, legacy_format::a8r8g8b8},
    {256u, 180u, legacy_format::a8r8g8b8}
}};

inline constexpr std::uint32_t dsr_dofrate_cb_runtime_shader_id = 0x0DF0u;

activation_decision evaluate_activation(const activation_context &context) noexcept;

constexpr bool preserves_temporal_state(const temporal_write_set &writes) noexcept
{
    return !writes.taa_history && !writes.velocity;
}

constexpr bool preserves_stock_depth(const temporal_write_set &writes) noexcept
{
    return !writes.stock_native_depth;
}

} // namespace dsrrl::operators::dof
