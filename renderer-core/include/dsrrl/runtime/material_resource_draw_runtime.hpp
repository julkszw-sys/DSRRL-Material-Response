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
    std::uint64_t diffuse_requests = 0;
    std::uint64_t normal_requests = 0;
    std::uint64_t fail_open = 0;
    bool quarantined = false;
};

struct prepared_material_resource_draw {
    std::array<
        island_draw_adapter_request,
        3> requests{};
    std::uint32_t request_count = 0;

    std::array<
        ID3D11ShaderResourceView *,
        3> retained_views{};
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

    bool prepare_draw_requests(
        ID3D11DeviceContext *context,
        std::uint32_t receiver_id,
        const operators::material_response::mtd_semantic_query &query,
        bool full_material_response_ready,
        prepared_material_resource_draw &prepared) noexcept;

    bool prepare_subsurface_body_requests(
        ID3D11DeviceContext *context,
        std::uint32_t target_plain_receiver_id,
        prepared_material_resource_draw &prepared,
        operators::resource_bridges::subsurface_body_texture &body_texture) noexcept;

    void release_prepared_draw(
        prepared_material_resource_draw &prepared) noexcept;

    material_resource_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    core::renderer_core &core_;
};

} // namespace dsrrl::runtime
