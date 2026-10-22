#pragma once

#include "dsrrl/core/feature_registry.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL material-response runtime bridge requires ReShade Add-on API 20
#endif

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace dsrrl::runtime {

struct material_response_runtime_telemetry {
    std::uint64_t mtd_seen = 0;
    std::uint64_t donor_registered = 0;
    std::uint64_t donor_unmapped = 0;
    std::uint64_t selector_seen = 0;
    std::uint64_t selector_donor = 0;
    std::uint64_t exact_pipeline_hits = 0;
    std::uint64_t alternate_pairs_ready = 0;
    std::uint64_t target_binds = 0;
    std::uint64_t replay_draws = 0;
    std::uint64_t c101_draws = 0;
    std::uint64_t c100_only_draws = 0;
    std::uint64_t b12_created = 0;
    std::uint64_t b12_cache_hits = 0;
    std::uint64_t deferred_fail_open = 0;
    std::uint64_t fail_open = 0;
    bool hooks_active = false;
    bool quarantined = false;
};

class material_response_runtime_bridge {
public:
    explicit material_response_runtime_bridge(
        core::feature_registry &features) noexcept;

    material_response_runtime_bridge(
        const material_response_runtime_bridge &) = delete;
    material_response_runtime_bridge &operator=(
        const material_response_runtime_bridge &) = delete;

    bool start() noexcept;
    void stop() noexcept;

    void on_init_device(
        reshade::api::device *device) noexcept;

    void on_destroy_device(
        reshade::api::device *device) noexcept;

    void on_init_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline_layout layout,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects,
        reshade::api::pipeline pipeline) noexcept;

    void on_destroy_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline pipeline) noexcept;

    void on_bind_pipeline(
        reshade::api::command_list *command_list,
        reshade::api::pipeline_stage stages,
        reshade::api::pipeline pipeline) noexcept;

    bool on_draw_indexed(
        reshade::api::command_list *command_list,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    material_response_runtime_telemetry telemetry() const noexcept;

private:
    struct pipeline_record {
        std::uint8_t host_index = 0;
        std::uint8_t receiver_id = 0;
    };

    core::feature_registry &features_;

    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, pipeline_record> pipelines_;
    std::unordered_map<std::uintptr_t, pipeline_record> bound_by_command_;

    std::atomic_bool hooks_active_{false};
    std::atomic_bool quarantined_{false};

    std::atomic<std::uint64_t> exact_pipeline_hits_{0};
    std::atomic<std::uint64_t> alternate_pairs_ready_{0};
    std::atomic<std::uint64_t> target_binds_{0};
    std::atomic<std::uint64_t> replay_draws_{0};
    std::atomic<std::uint64_t> c101_draws_{0};
    std::atomic<std::uint64_t> c100_only_draws_{0};
    std::atomic<std::uint64_t> deferred_fail_open_{0};
    std::atomic<std::uint64_t> fail_open_{0};
};

} // namespace dsrrl::runtime
