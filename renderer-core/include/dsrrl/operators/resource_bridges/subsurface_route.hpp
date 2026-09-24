#pragma once

#include <cstdint>
#include <string_view>

namespace dsrrl::operators::resource_bridges {

inline constexpr std::string_view k_dsr_body_subsurf_material =
    "Ps_Body[DSBT].mtd";
inline constexpr std::string_view k_dsr_body_subsurf_material_sha256 =
    "2706080f2b306245b90bf9d3fdfa38624b2ed0452a9b1aff830cf59de69b37d4";

inline constexpr std::string_view k_ptde_body_plain_material =
    "Ps_Body[DSB].mtd";
inline constexpr std::string_view k_ptde_body_plain_material_sha256 =
    "af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95";

enum class subsurface_body_texture : std::uint8_t {
    unknown = 0,
    bd_f_body_s,
    bd_m_body_s
};

enum class subsurface_route_action : std::uint8_t {
    preserve_host = 0,
    route_to_ptde_plain_difspcbmp_surface
};

enum class subsurface_bypass_carrier : std::uint8_t {
    none = 0,
    create_time_pixel_shader_substitution
};

enum class subsurface_route_reason : std::uint8_t {
    active = 0,
    material_not_verified,
    wrong_dsr_material_identity,
    receiver_not_verified,
    unsupported_receiver_identity,
    body_texture_not_verified,
    unsupported_body_texture,
    draw_path_not_verified,
    ptde_slot_mapping_not_verified,
    ptde_donor_not_verified,
    wrong_ptde_donor_identity,
    ptde_plain_surface_target_not_verified,
    target_plain_receiver_not_ready,
    spec_rgb_route_not_ready,
    diffuse_route_not_ready,
    normal_route_not_ready,
    material_response_route_not_ready,
    dsr_subsurf_bypass_carrier_not_ready
};

struct subsurface_route_context {
    bool actual_material_verified = false;
    std::string_view actual_material_name{};
    std::string_view actual_material_sha256{};

    bool actual_receiver_verified = false;
    std::string_view actual_receiver_name{};
    std::string_view actual_receiver_sha256{};

    bool actual_body_texture_verified = false;
    subsurface_body_texture body_spec_texture =
        subsurface_body_texture::unknown;

    // The exact receiver identities below are stable HemEnv/no-PointLight.
    // Runtime still has to prove the observed draw is on that same path.
    bool stable_hemenv_no_pointlight_draw_verified = false;

    // Slot-for-slot DSR Ps_Body[DSBT] -> PTDE Ps_Body[DSB] mapping.
    bool ptde_slot_mapping_verified = false;
    bool ptde_donor_verified = false;
    std::string_view ptde_material_name{};
    std::string_view ptde_material_sha256{};

    // Newer route-partition target: PTDE body is plain ColDifSpcBmp.
    // This explicit gate prevents the older compatibility-only
    // preserve-DSR-SSS route from becoming the final target by accident.
    bool ptde_plain_surface_target_verified = false;

    // The corresponding ordinary stable HemEnv receiver and the complete
    // already-certified PTDE surface route must exist before bypassing Subsurf.
    bool target_plain_receiver_ready = false;
    bool spec_rgb_route_ready = false;
    bool diffuse_route_ready = false;
    bool normal_route_ready = false;
    bool material_response_route_ready = false;

    // Carrier that actually excludes the DSR-only Subsurf/SSS contribution.
    bool dsr_subsurf_bypass_carrier_ready = false;
};

struct subsurface_route_decision {
    subsurface_route_action action =
        subsurface_route_action::preserve_host;
    subsurface_route_reason reason =
        subsurface_route_reason::material_not_verified;

    std::uint32_t target_plain_receiver_id = 0;
    std::string_view target_plain_receiver_name{};
    std::string_view target_plain_receiver_sha256{};
    subsurface_bypass_carrier carrier =
        subsurface_bypass_carrier::none;

    // Current PTDE route-partition target invariants.
    float ptde_c101 = 1.0f;
    bool bypass_dsr_subsurf = false;
    bool preserve_dsr_sss = true;
};

struct subsurface_receiver_route {
    std::string_view dsr_receiver_name;
    std::string_view dsr_receiver_sha256;
    std::uint32_t target_plain_receiver_id;
    std::string_view target_plain_receiver_name;
    std::string_view target_plain_receiver_sha256;
};

inline constexpr subsurface_receiver_route k_subsurface_receiver_routes[] = {
    {
        "FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo",
        "0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511",
        33u,
        "FRPG_Phn_DifSpcBmp______Csd_HemEnv.fpo",
        "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24"
    },
    {
        "FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo",
        "885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb",
        34u,
        "FRPG_Phn_DifSpcBmp______Sdw_HemEnv.fpo",
        "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837"
    },
    {
        "FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo",
        "3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860",
        35u,
        "FRPG_Phn_DifSpcBmp__________HemEnv.fpo",
        "7d03c75b69f5730eb741a4d327189d0bbed8a8450fb0ac04e1505f7b91763701"
    }
};

inline const subsurface_receiver_route *find_subsurface_receiver_route(
    std::string_view receiver_name,
    std::string_view receiver_sha256) noexcept
{
    for (const auto &route : k_subsurface_receiver_routes) {
        if (route.dsr_receiver_name == receiver_name &&
            route.dsr_receiver_sha256 == receiver_sha256)
            return &route;
    }

    return nullptr;
}

// High-confidence routing/authorization contract for the DSR-only body
// Subsurf fork.
//
// Exact PTDE mapping:
//   DSR  Ps_Body[DSBT].mtd / ColDifSpcBmpSubsurf
//   PTDE Ps_Body[DSB].mtd  / plain ColDifSpcBmp / c101 = 1.0
//
// Therefore the final PTDE-target candidate is not the older compatibility
// bridge that preserved DSR t10 SSS. It is an operator-isolated route into the
// corresponding ordinary PTDE DifSpcBmp surface island, with the DSR-only
// Subsurf/SSS contribution bypassed. Because that final route is currently
// HIGH CONFIDENCE rather than runtime/pixel-confirmed, the caller must provide
// an explicit target-verification gate.
//
// Static DXBC RE additionally proves the exact three Subsurf -> ordinary pairs
// are create-time pixel-shader ABI compatible: pairwise ISGN/OSGN are
// byte-identical, all five constant-buffer semantic layouts match, and the
// ordinary target resource declarations are a strict subset that removes only
// t10/s10 (gSMP_10 / gSMP_10Sampler). This authorizes create-time PS
// substitution/reuse as the narrow bypass carrier. It does NOT authorize the
// route unless the complete ordinary PTDE surface path is ready.
//
// Any incomplete identity, draw-path, surface-route or bypass carrier state
// fails open to the host path.
inline subsurface_route_decision evaluate_subsurface_route(
    const subsurface_route_context &context) noexcept
{
    subsurface_route_decision decision;

    if (!context.actual_material_verified) {
        decision.reason =
            subsurface_route_reason::material_not_verified;
        return decision;
    }

    if (context.actual_material_name != k_dsr_body_subsurf_material ||
        context.actual_material_sha256 !=
            k_dsr_body_subsurf_material_sha256) {
        decision.reason =
            subsurface_route_reason::wrong_dsr_material_identity;
        return decision;
    }

    if (!context.actual_receiver_verified) {
        decision.reason =
            subsurface_route_reason::receiver_not_verified;
        return decision;
    }

    const auto *receiver = find_subsurface_receiver_route(
        context.actual_receiver_name,
        context.actual_receiver_sha256);

    if (receiver == nullptr) {
        decision.reason =
            subsurface_route_reason::unsupported_receiver_identity;
        return decision;
    }

    decision.target_plain_receiver_id =
        receiver->target_plain_receiver_id;
    decision.target_plain_receiver_name =
        receiver->target_plain_receiver_name;
    decision.target_plain_receiver_sha256 =
        receiver->target_plain_receiver_sha256;

    if (!context.actual_body_texture_verified) {
        decision.reason =
            subsurface_route_reason::body_texture_not_verified;
        return decision;
    }

    if (context.body_spec_texture !=
            subsurface_body_texture::bd_f_body_s &&
        context.body_spec_texture !=
            subsurface_body_texture::bd_m_body_s) {
        decision.reason =
            subsurface_route_reason::unsupported_body_texture;
        return decision;
    }

    if (!context.stable_hemenv_no_pointlight_draw_verified) {
        decision.reason =
            subsurface_route_reason::draw_path_not_verified;
        return decision;
    }

    if (!context.ptde_slot_mapping_verified) {
        decision.reason =
            subsurface_route_reason::ptde_slot_mapping_not_verified;
        return decision;
    }

    if (!context.ptde_donor_verified) {
        decision.reason =
            subsurface_route_reason::ptde_donor_not_verified;
        return decision;
    }

    if (context.ptde_material_name != k_ptde_body_plain_material ||
        context.ptde_material_sha256 !=
            k_ptde_body_plain_material_sha256) {
        decision.reason =
            subsurface_route_reason::wrong_ptde_donor_identity;
        return decision;
    }

    if (!context.ptde_plain_surface_target_verified) {
        decision.reason =
            subsurface_route_reason::ptde_plain_surface_target_not_verified;
        return decision;
    }

    if (!context.target_plain_receiver_ready) {
        decision.reason =
            subsurface_route_reason::target_plain_receiver_not_ready;
        return decision;
    }

    if (!context.spec_rgb_route_ready) {
        decision.reason =
            subsurface_route_reason::spec_rgb_route_not_ready;
        return decision;
    }

    if (!context.diffuse_route_ready) {
        decision.reason =
            subsurface_route_reason::diffuse_route_not_ready;
        return decision;
    }

    if (!context.normal_route_ready) {
        decision.reason =
            subsurface_route_reason::normal_route_not_ready;
        return decision;
    }

    if (!context.material_response_route_ready) {
        decision.reason =
            subsurface_route_reason::material_response_route_not_ready;
        return decision;
    }

    if (!context.dsr_subsurf_bypass_carrier_ready) {
        decision.reason =
            subsurface_route_reason::dsr_subsurf_bypass_carrier_not_ready;
        return decision;
    }

    decision.action =
        subsurface_route_action::route_to_ptde_plain_difspcbmp_surface;
    decision.reason = subsurface_route_reason::active;
    decision.carrier =
        subsurface_bypass_carrier::create_time_pixel_shader_substitution;
    decision.bypass_dsr_subsurf = true;
    decision.preserve_dsr_sss = false;
    return decision;
}

} // namespace dsrrl::operators::resource_bridges
