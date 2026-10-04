#pragma once

#include <array>
#include <cstdint>

namespace dsrrl::operators::surface {

// Stable P_Metal-compatible FRPG_Phn_DifSpcBmp shadow strata recovered from
// vanilla DSR. Shader identity is necessary but never sufficient for
// activation: callers must additionally verify P_Metal material ownership.
enum class pmetal_shadow_receiver_variant : std::uint8_t {
    unsupported=0,
    plain_no_shadow,
    csd_no_point,
    sdw_no_point,
    csd_pnts,
    sdw_pnts,
    csd_pntss,
    sdw_pntss,
    csd_pntssss,
    sdw_pntssss
};

enum class pmetal_shadow_kernel_mode : std::uint8_t {
    none=0,
    dsr_comparison_pcf9,
    ptde_manual_packed_pcf16
};

struct pmetal_shadow_receiver_contract {
    pmetal_shadow_receiver_variant variant=
        pmetal_shadow_receiver_variant::unsupported;
    pmetal_shadow_kernel_mode stock_kernel=
        pmetal_shadow_kernel_mode::none;
    bool shadow_operator_present=false;
    bool needs_ptde_kernel_replacement=false;
    bool stock_regular_s7=false;
};

// Exact PTDE manual packed-depth kernel constants for the ordinary
// FrpgAsmModel/P_Metal-compatible shadow path.
inline constexpr std::array<float,3> k_pmetal_shadow_packed_depth_weights={{
    255.0f/256.0f,
    255.0f/65536.0f,
    255.0f/16777216.0f
}};
inline constexpr std::array<float,4> k_pmetal_shadow_pcf_offsets={{
    -1.5f,-0.5f,0.5f,1.5f
}};
inline constexpr float k_pmetal_shadow_texel_scale=1.0f/2048.0f;
inline constexpr float k_pmetal_shadow_pcf_weight=1.0f/16.0f;

// API-neutral semantic descriptor recovered from PTDE D3D9 sampler stage 7.
// D3D9 MAXMIPLEVEL is intentionally represented separately rather than
// pretending it is numerically identical to D3D11 MaxLOD.
enum class pmetal_shadow_filter : std::uint8_t {
    point=1
};

enum class pmetal_shadow_address_mode : std::uint8_t {
    clamp=3
};

struct pmetal_shadow_sampler_descriptor {
    bool comparison=false;
    pmetal_shadow_filter min_filter=pmetal_shadow_filter::point;
    pmetal_shadow_filter mag_filter=pmetal_shadow_filter::point;
    pmetal_shadow_filter mip_filter=pmetal_shadow_filter::point;
    pmetal_shadow_address_mode address_u=pmetal_shadow_address_mode::clamp;
    pmetal_shadow_address_mode address_v=pmetal_shadow_address_mode::clamp;
    pmetal_shadow_address_mode address_w=pmetal_shadow_address_mode::clamp;
    std::uint32_t border_color=0xffffffffu;
    std::uint32_t max_anisotropy=1u;
    std::uint32_t ptde_d3d9_max_mip_level=0u;
};

inline constexpr pmetal_shadow_sampler_descriptor
    k_ptde_pmetal_shadow_s7{};

bool pmetal_shadow_sampler_matches_ptde(
    const pmetal_shadow_sampler_descriptor &observed) noexcept;

pmetal_shadow_receiver_contract pmetal_shadow_contract_for_shader(
    std::uint32_t shader_index) noexcept;

enum class pmetal_shadow_runtime_reason : std::uint8_t {
    ready=0,
    receiver_not_verified,
    material_not_verified,
    unsupported_receiver,
    no_shadow_operator,
    replacement_shader_not_ready,
    stock_ptde_kernel_identity_not_verified,
    runtime_t7_identity_not_verified,
    runtime_t7_raw_rgb_not_verified,
    regular_s7_sampler_not_ready,
    regular_s7_descriptor_mismatch,
    stock_regular_s7_not_verified,
    sampler_override_transaction_not_ready,
    draw_transaction_not_ready
};

struct pmetal_shadow_runtime_context {
    bool receiver_verified=false;
    bool material_pmetal_verified=false;
    std::uint32_t shader_index=0u;

    // Required only for no-Point/PntS comparison-kernel hosts.
    bool replacement_ptde_kernel_shader_ready=false;

    // Required for fixed PntSS/PntSSSS certification.
    bool stock_ptde_kernel_identity_verified=false;

    // Static t7 RDEF equality is not enough. These gates refer to the actual
    // draw-local SRV/resource identity and the ability to read the packed RGB
    // representation consumed by the manual PTDE kernel.
    bool runtime_t7_identity_verified=false;
    bool runtime_t7_raw_rgb_verified=false;

    // Replacement path uses a regular sampler carrying the exact PTDE stage-7
    // semantics. The shared draw transaction must restore the prior s7 state.
    bool regular_s7_sampler_ready=false;
    pmetal_shadow_sampler_descriptor regular_s7_descriptor{};
    bool stock_regular_s7_verified=false;
    bool sampler_override_transaction_ready=false;
    bool draw_transaction_ready=false;
};

struct pmetal_shadow_runtime_plan {
    bool ready=false;
    pmetal_shadow_runtime_reason reason=
        pmetal_shadow_runtime_reason::receiver_not_verified;

    bool keep_live_t7=true;
    bool replace_t7_resource=false;
    bool replace_comparison_kernel=false;
    bool preserve_stock_ptde_style_kernel=false;
    bool override_s7_with_ptde_regular_sampler=false;
};

pmetal_shadow_runtime_plan evaluate_pmetal_shadow_runtime_readiness(
    const pmetal_shadow_runtime_context &context) noexcept;

} // namespace dsrrl::operators::surface
