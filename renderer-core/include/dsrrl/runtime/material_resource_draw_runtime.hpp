#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/operators/resource_bridges/subsurface_route.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL material resource draw runtime requires ReShade Add-on API 20
#endif

#include <array>
#include <cstdint>

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime {

struct material_resource_telemetry {
    std::uint64_t named_views = 0;
    std::uint64_t sidecar_ready = 0;
    std::uint64_t sidecar_missing = 0;
    std::uint64_t sidecar_unsupported = 0;
    std::uint64_t spec_requests = 0;
    std::uint64_t fixed_pointlight_spec_requests = 0;
    std::uint64_t fixed_pointlight_diffuse_requests = 0;
    std::uint64_t diffuse_requests = 0;
    std::uint64_t normal_requests = 0;
    std::uint64_t fail_open = 0;
    bool quarantined = false;
};

struct specular_companion_probe {
    bool context_valid = false;
    bool quarantined = false;
    bool stock_bound = false;
    bool snapshot_resolved = false;
    std::uint64_t logical_hash = 0;
    bool logical_hash_allowed = false;
    bool companion_ready = false;

    bool ready() const noexcept
    {
        return context_valid &&
            !quarantined &&
            stock_bound &&
            snapshot_resolved &&
            logical_hash != 0u &&
            logical_hash_allowed &&
            companion_ready;
    }
};

struct prepared_material_resource_draw {
    std::array<
        island_draw_adapter_request,
        4> requests{};
    std::uint32_t request_count = 0;

    std::array<
        ID3D11ShaderResourceView *,
        6> retained_views{};
    std::uint32_t retained_count = 0;

    bool spec_rgb = false;
    bool diffuse = false;
    bool normal = false;
};

class material_resource_draw_runtime {
public:
    explicit material_resource_draw_runtime(
        core::renderer_core &core) noexcept;
    ~material_resource_draw_runtime();

    material_resource_draw_runtime(
        const material_resource_draw_runtime &) = delete;
    material_resource_draw_runtime &operator=(
        const material_resource_draw_runtime &) = delete;

    bool register_events() noexcept;
    void unregister_events() noexcept;

    // Draw-local authority for shared MR profiles whose MTD name is reused
    // outside the certified PTDE companion route. Checks the currently bound
    // stock t1 logical identity and requires an actual PTDE SpecRGB companion.
    specular_companion_probe probe_exact_specular_companion(
        ID3D11DeviceContext *context) noexcept;

    bool exact_specular_companion_ready(
        ID3D11DeviceContext *context) noexcept;

    // SPC25 opt-in only: recover the *currently bound native stock t1*
    // after creation, if it acquired a canonical exact D3D11 debug name
    // later than init_resource_view. Never derive texture identity from
    // MTD/FLVER alone or infer it from GPU format/dimensions.
    bool try_recover_exact_bound_spec_from_native_name(
        ID3D11DeviceContext *context,
        bool exact_c5330_route14_test = false) noexcept;

    bool prepare_draw_requests(
        ID3D11DeviceContext *context,
        std::uint32_t receiver_id,
        const operators::material_response::mtd_semantic_query &query,
        bool full_material_response_ready,
        bool spec_rgb_consumer_ready,
        prepared_material_resource_draw &prepared) noexcept;

    // Runtime v2 fast path. Uses already-shadowed stock t0/t1/t2 bindings
    // and avoids D3D11 PSGetShaderResources on qualifying draws.
    bool prepare_draw_requests_bound(
        ID3D11ShaderResourceView *const (&views)[3],
        std::uint32_t receiver_id,
        const operators::material_response::mtd_semantic_query &query,
        bool full_material_response_ready,
        bool spec_rgb_consumer_ready,
        prepared_material_resource_draw &prepared) noexcept;

    bool prepare_fixed_pointlight_material_requests(
        ID3D11DeviceContext *context,
        const operators::material_response::mtd_semantic_query &query,
        bool exact_fixed_receiver_verified,
        bool direct_pointlight_material_authorized,
        bool blended_material,
        prepared_material_resource_draw &prepared) noexcept;

    bool prepare_subsurface_body_requests(
        ID3D11DeviceContext *context,
        std::uint32_t target_plain_receiver_id,
        prepared_material_resource_draw &prepared,
        operators::resource_bridges::subsurface_body_texture &body_texture) noexcept;

    bool drop_spec_rgb_request(
        prepared_material_resource_draw &prepared) noexcept;

    // Retain only the exact equipment SpecRGB sidecar request. Used by
    // consumer-local diagnostics that must not also activate diffuse/normal
    // sidecars from the broader material-response resource bundle.
    bool keep_only_spec_rgb_request(
        prepared_material_resource_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_material_resource_draw &prepared) noexcept;

    material_resource_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    core::renderer_core &core_;
};

} // namespace dsrrl::runtime
