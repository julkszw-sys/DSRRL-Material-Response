#pragma once

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL fixed PointLight runtime requires ReShade Add-on API 20
#endif

#include <cstdint>

struct ID3D11Buffer;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime {

struct fixed_pointlight_telemetry {
    std::uint64_t producer_captures = 0;
    std::uint64_t producer_restarts = 0;
    std::uint64_t producer_rejects = 0;
    std::uint64_t selector_seen = 0;
    std::uint64_t selector_match = 0;
    std::uint64_t selector_stale = 0;
    std::uint64_t t19_create = 0;
    std::uint64_t t19_hit = 0;
    std::uint64_t requests = 0;
    bool capture_hook_armed = false;
    bool quarantined = false;
    bool restore_failed = false;
};

struct prepared_fixed_pointlight_draw {
    ID3D11ShaderResourceView *t19 = nullptr;
    std::uint64_t producer_serial = 0;
    std::uint8_t captured_light_count = 0;
    bool owner_verified = false;
    bool producer_serial_fresh = false;
    bool ready = false;
};

// Called by the already-owned FLVER/material selector bridge. Inert when the
// fixed PointLight runtime is not installed.
void fixed_pointlight_selector_event_bridge(void *owner) noexcept;

// Cheap non-consuming source-authority gate. Receiver/material work must not
// start until an exact fresh 2-light/4-light producer snapshot already exists.
bool fixed_pointlight_source_ready_bridge(
    std::uint8_t expected_count) noexcept;

class fixed_pointlight_draw_runtime {
public:
    fixed_pointlight_draw_runtime() noexcept = default;

    bool install() noexcept;
    void uninstall() noexcept;

    void selector_event(void *owner) noexcept;

    // Non-consuming source authority check. This is intentionally independent
    // of receiver material identity; it only verifies the already captured
    // fixed source snapshot for the expected PntSS/PntSSSS light count.
    bool source_ready(
        std::uint8_t expected_count) const noexcept;

    // expected_count is receiver-derived: PntSS=2, PntSSSS=4.
    bool prepare_t19(
        ID3D11DeviceContext *context,
        std::uint8_t expected_count,
        prepared_fixed_pointlight_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_fixed_pointlight_draw &prepared) noexcept;

    void consume_draw_selection() noexcept;
    void on_destroy_device(reshade::api::device *device) noexcept;

    fixed_pointlight_telemetry telemetry() const noexcept;
    void reset() noexcept;
};

} // namespace dsrrl::runtime
