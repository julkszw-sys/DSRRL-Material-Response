#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/material_response_draw_transaction.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"

#include <reshade.hpp>
#include <cstdint>

namespace dsrrl::runtime {

struct prepared_subsurface_draw {
    prepared_material_response_draw mr{};
    prepared_material_resource_draw resources{};
    island_draw_adapter_request subsurface{};
    std::uint32_t target_receiver_id = 0;
    bool ready = false;
};

struct subsurface_draw_telemetry {
    std::uint64_t candidates = 0;
    std::uint64_t material_rejects = 0;
    std::uint64_t pipeline_rejects = 0;
    std::uint64_t surface_rejects = 0;
    std::uint64_t prepared = 0;
};

class subsurface_draw_runtime {
public:
    subsurface_draw_runtime(
        core::renderer_core &core,
        material_response_draw_runtime &mr,
        material_resource_draw_runtime &resources) noexcept;

    bool prepare(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        prepared_subsurface_draw &prepared) noexcept;

    void release(
        prepared_subsurface_draw &prepared) noexcept;

    subsurface_draw_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    core::renderer_core &core_;
    material_response_draw_runtime &mr_;
    material_resource_draw_runtime &resources_;

    std::uint64_t candidates_ = 0;
    std::uint64_t material_rejects_ = 0;
    std::uint64_t pipeline_rejects_ = 0;
    std::uint64_t surface_rejects_ = 0;
    std::uint64_t prepared_ = 0;
};

} // namespace dsrrl::runtime
