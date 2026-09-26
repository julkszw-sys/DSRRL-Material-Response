#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/operators/lightbank/snapshot_freshness.hpp"

#include <reshade.hpp>

#include <array>
#include <atomic>
#include <cstdint>

struct ID3D11Buffer;
struct ID3D11DeviceContext;

namespace dsrrl::runtime {

struct upper_lower_telemetry {
    std::uint64_t wrapper5 = 0;
    std::uint64_t wrapper6 = 0;
    std::uint64_t steady_seen = 0;
    std::uint64_t steady_pass = 0;
    std::uint64_t blend_seen = 0;
    std::uint64_t blend_upper = 0;
    std::uint64_t blend_lower = 0;
    std::uint64_t d123_steady = 0;
    std::uint64_t d123_blend_direction = 0;
    std::uint64_t d123_blend_color = 0;
    std::uint64_t d123_snapshot_publish = 0;
    std::uint64_t snapshot_publish = 0;
    std::uint64_t selector_seen = 0;
    std::uint64_t selector_match = 0;
    std::uint64_t selector_miss = 0;
    std::uint64_t tuple_mismatch = 0;
    std::uint64_t b13_create = 0;
    std::uint64_t b13_hit = 0;
    std::uint64_t hemdir3_b13_create = 0;
    std::uint64_t hemdir3_b13_hit = 0;
    std::uint64_t requests = 0;
    std::uint64_t hemdir3_carrier_requests = 0;
    std::uint64_t pmetal_env_steady = 0;
    std::uint64_t pmetal_env_blend = 0;
    std::uint64_t pmetal_env_miss = 0;
    bool producer_hooks_armed = false;
    bool pmetal_env_hook_armed = false;
    bool quarantined = false;
    bool restore_failed = false;
};

struct prepared_upper_lower_draw {
    island_draw_adapter_request request{};
    ID3D11Buffer *b13 = nullptr;
    bool ready = false;
};

struct pmetal_env_source {
    std::array<float,3> a{};
    std::array<float,3> b{};
    float beta = 0.0f;
    std::uint64_t bank_signature_a = 0;
    std::uint64_t bank_signature_b = 0;
    std::uint32_t row_id_a = 0;
    std::uint32_t row_id_b = 0;
};

struct prepared_hemdir3_carrier {
    ID3D11Buffer *b13 = nullptr;
    operators::lightbank::lightbank_snapshot_fingerprint fingerprint{};
    bool d123_ready = false;
    bool upper_lower_ready = false;
    bool ready = false;
};

// Selector bridge called by the already-owned exact FLVER/material selector
// detour. It is inert unless an upper_lower_draw_runtime instance is installed.
void upper_lower_selector_event_bridge(
    void *owner,
    void *return_address,
    void *r14,
    void *r15) noexcept;

class upper_lower_draw_runtime {
public:
    explicit upper_lower_draw_runtime(
        core::renderer_core &core) noexcept;

    bool install() noexcept;
    void uninstall() noexcept;

    // Fed by the already-owned exact material selector hook at 0x22BA20.
    void selector_event(
        void *owner,
        void *return_address,
        void *r14,
        void *r15) noexcept;

    bool prepare_upper_lower_carrier(
        ID3D11DeviceContext *context,
        prepared_upper_lower_draw &prepared) noexcept;

    bool prepare_draw_request(
        ID3D11DeviceContext *context,
        std::uint32_t receiver_id,
        prepared_upper_lower_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_upper_lower_draw &prepared) noexcept;

    bool prepare_hemdir3_carrier(
        ID3D11DeviceContext *context,
        prepared_hemdir3_carrier &prepared) noexcept;

    void release_hemdir3_carrier(
        prepared_hemdir3_carrier &prepared) noexcept;

    bool selected_pmetal_env_source(
        pmetal_env_source &out) const noexcept;

    // Selection is one draw-scoped semantic event. Never carry it forward.
    void consume_draw_selection() noexcept;

    void on_destroy_device(
        reshade::api::device *device) noexcept;

    upper_lower_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    core::renderer_core &core_;
};

} // namespace dsrrl::runtime
