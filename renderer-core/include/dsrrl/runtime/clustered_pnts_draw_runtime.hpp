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
    // R52 source-only producer telemetry. These counters are independent of
    // the retired clustered receiver/material path.
    std::uint64_t source_producer_hits = 0;
    std::uint64_t source_coverage_accept = 0;
    std::uint64_t source_coverage_reject = 0;
    std::uint64_t source_class_reject = 0;
    std::uint64_t source_authority_cache_hit = 0;
    std::uint64_t source_authority_cache_miss = 0;
    std::uint64_t source_category0 = 0;
    std::uint64_t source_category1 = 0;
    std::uint64_t source_category2 = 0;
    std::uint64_t source_category3 = 0;
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
    std::uint64_t prepare_neutral_empty = 0;
    std::uint64_t prepare_precondition_fail = 0;
    std::uint64_t selection_fail = 0;
    std::uint64_t selection_empty = 0;
    std::uint64_t sidecar_build_fail = 0;
    std::uint64_t context_immediate = 0;
    std::uint64_t context_deferred = 0;
    std::uint64_t context_other = 0;
    std::uint64_t gpu_prepare_fail = 0;
    std::uint64_t upload_fail = 0;
    bool enabled = false;
    bool quarantined = false;
};

enum class clustered_pnts_prepare_failure : std::uint8_t {
    none = 0,
    precondition,
    selection,
    empty_selection,
    source_capture,
    sidecar_build,
    gpu_prepare,
    gpu_resources,
    upload
};

struct prepared_clustered_pnts_draw {
    ID3D11ShaderResourceView *t18 = nullptr;
    ID3D11ShaderResourceView *t19 = nullptr;
    ID3D11Buffer *b12 = nullptr;
    std::uint64_t producer_serial = 0;
    std::uint8_t raw_selected_count = 0;
    std::uint32_t material_max_pnt_lit_num = 0;
    std::uint8_t effective_count = 0;
    bool owner_verified = false;
    bool selector_mirror_verified = false;
    bool neutral_no_pointlights = false;
    // R29 fast path: t18/t19/b12 are borrowed from a TLS cache that owns
    // stable COM references across draws. release_prepared_draw() must not
    // churn AddRef/Release for borrowed carriers.
    bool carrier_borrowed_tls = false;
    bool ready = false;
    clustered_pnts_prepare_failure failure =
        clustered_pnts_prepare_failure::none;
    std::uint8_t sidecar_result_code = 0u;
};

void clustered_pnts_builder_event_bridge(
    void *draw,
    void *renderer_context) noexcept;

void clustered_pnts_selector_event_bridge(
    void *owner,
    const void *actual_material) noexcept;

bool clustered_pnts_selector_handoff_ready_bridge(
    void *owner) noexcept;

// Source/carrier production is a separate stage from material-response
// authorization. This keeps PointLight source semantics independently reusable
// without implicitly authorizing any Spc local-specular implementation.
void clustered_pnts_selector_source_event_bridge() noexcept;

void clustered_pnts_selector_identity_event_bridge(
    const operators::material_response::material_identity &identity,
    bool expected_spc) noexcept;

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

    void selector_source_event() noexcept;

    // Advances the source-carrier cache epoch once per presented frame.
    // Cached entries remain thread-local and are reused only while their
    // exact source-object state snapshot is unchanged.
    void frame_event(
        std::uint64_t frame_serial) noexcept;

    void selector_identity_event(
        const operators::material_response::material_identity &identity,
        bool expected_spc) noexcept;

    bool current_draw_authority(
        bool expected_spc,
        operators::material_response::material_identity &material,
        operators::material_response::decision &decision) const noexcept;

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
