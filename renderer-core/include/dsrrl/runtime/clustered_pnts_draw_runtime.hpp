#pragma once

#include "dsrrl/operators/material_response/material_response_island.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL clustered PntS runtime requires ReShade Add-on API 20
#endif

#include <cstdint>

struct ID3D11Buffer;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime {

struct clustered_pnts_telemetry {
    std::uint64_t builder_seen = 0;
    std::uint64_t collection_ok = 0;
    std::uint64_t collection_fail = 0;
    std::uint64_t selector_calls = 0;
    std::uint64_t mirror_equal = 0;
    std::uint64_t mirror_diff = 0;
    std::uint64_t source_capture_ok = 0;
    std::uint64_t source_capture_fail = 0;
    std::uint64_t snapshot_publish = 0;
    std::uint64_t selector_seen = 0;
    std::uint64_t owner_join_hit = 0;
    std::uint64_t owner_join_miss = 0;
    std::uint64_t material_limit_ok = 0;
    std::uint64_t material_limit_fail = 0;
    std::uint64_t sidecar_ready = 0;
    std::uint64_t sidecar_fail = 0;
    std::uint64_t t18_create = 0;
    std::uint64_t t18_hit = 0;
    std::uint64_t t19_create = 0;
    std::uint64_t t19_hit = 0;
    std::uint64_t b12_create = 0;
    std::uint64_t b12_hit = 0;
    std::uint64_t prepare_ok = 0;
    std::uint64_t prepare_fail = 0;
    bool enabled = false;
    bool quarantined = false;
};

struct prepared_clustered_pnts_draw {
    ID3D11ShaderResourceView *t18 = nullptr;
    ID3D11ShaderResourceView *t19 = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::uint64_t producer_serial = 0;
    std::uint8_t raw_selected_count = 0;
    std::uint8_t material_max_pnt_lit_num = 0;
    std::uint8_t effective_count = 0;
    bool owner_verified = false;
    bool selector_mirror_verified = false;
    bool ready = false;
};

void clustered_pnts_builder_event_bridge(
    void *draw,
    void *renderer_context) noexcept;

void clustered_pnts_selector_event_bridge(
    void *owner,
    const void *actual_material) noexcept;

class clustered_pnts_draw_runtime {
public:
    clustered_pnts_draw_runtime() noexcept = default;

    bool install() noexcept;
    void uninstall() noexcept;

    void builder_event(
        void *draw,
        void *renderer_context) noexcept;

    void selector_event(
        void *owner,
        const void *actual_material) noexcept;

    bool prepare_sidecar(
        ID3D11DeviceContext *context,
        const operators::material_response::decision &material,
        prepared_clustered_pnts_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_clustered_pnts_draw &prepared) noexcept;

    void consume_draw_selection() noexcept;

    void on_destroy_device(
        reshade::api::device *device) noexcept;

    clustered_pnts_telemetry telemetry() const noexcept;
    void reset() noexcept;
};

} // namespace dsrrl::runtime
