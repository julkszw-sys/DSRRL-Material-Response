#pragma once

#include "dsrrl/core/feature_registry.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL Material Response runtime requires ReShade Add-on API 20
#endif

#include <atomic>
#include <cstdint>
#include <memory>

namespace dsrrl::runtime {

struct material_response_runtime_telemetry {
    std::uint64_t mtd_seen = 0;
    std::uint64_t mtd_mapped = 0;
    std::uint64_t mtd_unmapped = 0;
    std::uint64_t selector_seen = 0;
    std::uint64_t selector_resolved = 0;
    std::uint64_t target_binds = 0;
    std::uint64_t ptde_draws = 0;
    std::uint64_t c101_draws = 0;
    std::uint64_t c100_only_draws = 0;
    std::uint64_t b12_create = 0;
    std::uint64_t b12_hit = 0;
    std::uint64_t fail_open = 0;
    std::uint64_t hook_guard_pass = 0;
    std::uint64_t hook_guard_fail = 0;
    bool quarantined = false;
    bool hooks_active = false;
};

class material_response_runtime {
public:
    explicit material_response_runtime(
        core::feature_registry &features) noexcept;
    ~material_response_runtime();

    material_response_runtime(
        const material_response_runtime &) = delete;
    material_response_runtime &operator=(
        const material_response_runtime &) = delete;

    bool initialize() noexcept;
    void shutdown() noexcept;

    void on_init_device(
        reshade::api::device *device) noexcept;
    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool on_create_pipeline(
        reshade::api::device *device,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects) noexcept;

    void on_init_pipeline(
        reshade::api::device *device,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects,
        reshade::api::pipeline pipeline) noexcept;

    void on_destroy_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline pipeline) noexcept;

    void on_bind_pipeline(
        reshade::api::command_list *cmd,
        reshade::api::pipeline_stage stages,
        reshade::api::pipeline pipeline) noexcept;

    bool on_draw(
        reshade::api::command_list *cmd) noexcept;

    bool on_draw_indexed(
        reshade::api::command_list *cmd,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    material_response_runtime_telemetry telemetry() const noexcept;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
    core::feature_registry &features_;
};

} // namespace dsrrl::runtime
