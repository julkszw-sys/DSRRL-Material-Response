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
    std::uint64_t reference_publish_tuple_mismatch = 0;
    std::uint64_t b13_create = 0;
    std::uint64_t b13_hit = 0;
    std::uint64_t hemdir3_b13_create = 0;
    std::uint64_t hemdir3_b13_hit = 0;
    std::uint64_t requests = 0;
    std::uint64_t hemdir3_carrier_requests = 0;
    std::uint64_t pmetal_env_steady = 0;
    std::uint64_t pmetal_env_blend = 0;
    std::uint64_t pmetal_env_miss = 0;
    std::uint64_t direct_ul_steady_inject = 0;
    std::uint64_t direct_ul_blend_inject = 0;
    std::uint64_t direct_ul_inject_fail = 0;
    std::uint64_t direct_ul_draw_ready = 0;
    std::uint64_t direct_ul_draw_fallback = 0;
    bool producer_hooks_armed = false;
    bool pmetal_env_hook_armed = false;
    bool direct_ul_producer_active = false;
    bool direct_ul_operator_changed = false;
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

enum class pmetal_env_source_diag_status : std::uint32_t {
    none = 0u,
    success,
    token_invalid,
    base_null,
    header_invalid,
    selector_oob,
    signature_invalid,
    bank_unknown,
    row_read_failed,
    row_unknown,
    nonfinite
};

struct pmetal_env_source_diag_endpoint {
    pmetal_env_source_diag_status status =
        pmetal_env_source_diag_status::none;
    std::int32_t selector = -1;
    std::uint16_t bank_count = 0u;
    std::uint64_t bank_signature = 0u;
    std::uint32_t row_id = 0u;
};

struct pmetal_env_source_diagnostic {
    pmetal_env_source_diag_endpoint a{};
    pmetal_env_source_diag_endpoint b{};
    float beta = 0.0f;
    bool observed = false;

    // Reference-carrier transport frontier. Thread IDs are captured once per
    // session so cross-thread producer/selector routing can be falsified
    // without enabling heavyweight hot telemetry.
    std::uint32_t producer_publish_tid = 0u;
    std::uint32_t selector_tid = 0u;
    bool producer_publish_seen = false;
    bool selector_relevant_seen = false;
    bool selector_candidate_found = false;
    bool selector_tuple_read = false;
    bool selector_tuple_match = false;
    bool draw_token_selected = false;
    bool draw_token_source_ready = false;
    bool draw_token_vectors_ready = false;
    bool draw_token_upper_lower_ready = false;

    // Exact selector->producer/draw handoff diagnostics. These distinguish
    // successful global selector authentication from consumption on the
    // producer/draw thread without making thread identity an authority by
    // itself: producer_serial must also match exactly.
    std::uint32_t source_consumer_tid = 0u;
    std::uint64_t source_consumer_serial = 0u;
    std::uint64_t cross_thread_selected_publish = 0u;
    std::uint64_t cross_thread_draw_consume = 0u;
    std::uint64_t cross_thread_serial_miss = 0u;
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

    // reference_only=true arms only the exact LightBank producer/selector
    // reference transport required by downstream operators such as P_Metal
    // EnvSpec. It does not decode/materialize U/L/D123 producer payloads.
    bool install(bool reference_only = false) noexcept;
    void uninstall() noexcept;

    // Fed by the already-owned exact material selector hook at 0x22BA20.
    void selector_event(
        void *owner,
        void *return_address,
        void *r14,
        void *r15) noexcept;

    // True only when both steady and blended PTDE-linear U/L producer
    // injection are construction-armed. Integrated routing may bypass the
    // draw-time U/L shader+b13 replay only while this remains true.
    bool direct_producer_active() const noexcept;

    // Draw-local activation proof. Hook liveness alone is not sufficient:
    // true means the selected LightBank tuple actually received the direct
    // PTDE U/L producer write for this draw.
    bool direct_producer_ready_for_draw() const noexcept;

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

    pmetal_env_source_diagnostic
    pmetal_source_diagnostic() const noexcept;

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
