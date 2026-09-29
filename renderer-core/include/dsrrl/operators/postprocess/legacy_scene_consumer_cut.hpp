#pragma once

#include <cstdint>

namespace dsrrl::operators::postprocess {

// Semantic cut shared by legacy Bloom and legacy HDR.
//
// The Bloom/HDR island owns the representation expected by its consumers, not
// the complete provenance of every upstream scene writer. Upstream renderer
// islands remain responsible for producing PTDE-equivalent scene code. This
// bridge only authenticates the current-frame scene value at the consumer cut
// and the PTDE Q8 storage boundary.
//
// Consequently, main-FLVER / FX-SFX writer recurrence is deliberately not part
// of this carrier. That proof remains useful for full PTDE scene-history
// reconstruction diagnostics, but it must not become a Bloom/HDR-local
// activation prerequisite when stock DSR SFX is preserved as a separate island.
enum class legacy_scene_cut_domain : std::uint8_t {
    unknown = 0,

    // Stock DSR late scene signal. This is not automatically equivalent to the
    // PTDE scene code consumed by legacy Bloom/HDR.
    dsr_decoded_linear,

    // A producer-side bridge has already established PTDE-equivalent numeric
    // scene code, but it has not yet crossed the legacy 8-bit storage boundary.
    ptde_scene_code_float,

    // Exact legacy normalized scene storage.
    ptde_q8_unorm
};

enum class legacy_scene_q8_transfer : std::uint8_t {
    unknown = 0,

    // Source is already PTDE Q8 normalized storage.
    identity_q8,

    // PTDE float scene code crosses the legacy normalized 8-bit RT boundary.
    // The relevant operator is storage saturation/quantization only. No
    // arbitrary gain, gamma, exposure or tone-map compensation belongs here.
    sat_quantize_unorm8
};

enum class legacy_scene_consumer_cut_result : std::uint8_t {
    exact_construction = 0,
    source_domain_not_ptde_equivalent,
    current_frame_source_not_verified,
    selected_scene_route_not_verified,
    q8_transfer_not_closed,
    q8_storage_not_verified,
    extra_color_operator_present,
    bloom_consumer_not_verified,
    hdr_consumer_not_verified
};

struct legacy_scene_consumer_cut_carrier {
    legacy_scene_cut_domain source_domain =
        legacy_scene_cut_domain::unknown;
    legacy_scene_q8_transfer transfer =
        legacy_scene_q8_transfer::unknown;

    bool current_frame_source_verified = false;
    bool selected_scene_route_verified = false;
    bool q8_b8g8r8a8_unorm_storage_verified = false;

    // Explicit negative proof at this cut. Domain conversion belongs upstream;
    // once PTDE scene code reaches this cut, the storage bridge must not invent
    // gain/gamma/exposure/tone-map compensation.
    bool no_extra_gain_gamma_exposure = false;

    bool bloom_consumer_verified = false;
    bool hdr_consumer_verified = false;
};

inline legacy_scene_consumer_cut_result
validate_legacy_scene_q8_cut_common(
    const legacy_scene_consumer_cut_carrier &c) noexcept
{
    if (c.source_domain !=
            legacy_scene_cut_domain::ptde_scene_code_float &&
        c.source_domain !=
            legacy_scene_cut_domain::ptde_q8_unorm)
        return legacy_scene_consumer_cut_result::
            source_domain_not_ptde_equivalent;

    if (!c.current_frame_source_verified)
        return legacy_scene_consumer_cut_result::
            current_frame_source_not_verified;

    if (!c.selected_scene_route_verified)
        return legacy_scene_consumer_cut_result::
            selected_scene_route_not_verified;

    const bool transfer_exact =
        (c.source_domain ==
             legacy_scene_cut_domain::ptde_scene_code_float &&
         c.transfer ==
             legacy_scene_q8_transfer::sat_quantize_unorm8) ||
        (c.source_domain ==
             legacy_scene_cut_domain::ptde_q8_unorm &&
         c.transfer ==
             legacy_scene_q8_transfer::identity_q8);

    if (!transfer_exact)
        return legacy_scene_consumer_cut_result::
            q8_transfer_not_closed;

    if (!c.q8_b8g8r8a8_unorm_storage_verified)
        return legacy_scene_consumer_cut_result::
            q8_storage_not_verified;

    if (!c.no_extra_gain_gamma_exposure)
        return legacy_scene_consumer_cut_result::
            extra_color_operator_present;

    return legacy_scene_consumer_cut_result::exact_construction;
}

inline legacy_scene_consumer_cut_result
validate_bloom_scene_consumer_cut(
    const legacy_scene_consumer_cut_carrier &c) noexcept
{
    const auto common =
        validate_legacy_scene_q8_cut_common(c);
    if (common !=
        legacy_scene_consumer_cut_result::exact_construction)
        return common;

    if (!c.bloom_consumer_verified)
        return legacy_scene_consumer_cut_result::
            bloom_consumer_not_verified;

    return legacy_scene_consumer_cut_result::exact_construction;
}

inline legacy_scene_consumer_cut_result
validate_hdr_scene_consumer_cut(
    const legacy_scene_consumer_cut_carrier &c) noexcept
{
    const auto common =
        validate_legacy_scene_q8_cut_common(c);
    if (common !=
        legacy_scene_consumer_cut_result::exact_construction)
        return common;

    if (!c.hdr_consumer_verified)
        return legacy_scene_consumer_cut_result::
            hdr_consumer_not_verified;

    return legacy_scene_consumer_cut_result::exact_construction;
}

} // namespace dsrrl::operators::postprocess
