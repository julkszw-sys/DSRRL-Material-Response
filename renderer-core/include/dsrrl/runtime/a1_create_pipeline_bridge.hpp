#pragma once

#include "dsrrl/core/feature_registry.hpp"
#include "dsrrl/operators/legacy_plan/a1_create_time_materializer.hpp"
#include "dsrrl/operators/legacy_plan/build151_nospc_extension.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL integrated A1 bridge requires ReShade Add-on API 20
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dsrrl::runtime {

struct a1_runtime_telemetry {
    std::uint64_t create_events = 0;
    std::uint64_t candidate_size_hits = 0;
    std::uint64_t exact_identity_hits = 0;
    std::uint64_t materialized = 0;
    std::uint64_t pass_unknown_identity = 0;
    std::uint64_t pass_no_enabled_owner = 0;
    std::uint64_t fail_open = 0;
    std::uint64_t init_attested = 0;
    std::uint64_t init_mismatch = 0;
    std::uint64_t target_binds = 0;
    std::uint64_t build151_nospc_exact_hits = 0;
    std::uint64_t build151_nospc_materialized = 0;
    std::uint64_t build151_nospc_binds = 0;
    bool quarantined = false;
};

class a1_create_pipeline_bridge {
public:
    explicit a1_create_pipeline_bridge(
        core::feature_registry &features) noexcept;

    a1_create_pipeline_bridge(
        const a1_create_pipeline_bridge &) = delete;
    a1_create_pipeline_bridge &operator=(
        const a1_create_pipeline_bridge &) = delete;

    void on_init_device(
        reshade::api::device *device) noexcept;

    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool on_create_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline_layout layout,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects) noexcept;

    void on_init_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline_layout layout,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects,
        reshade::api::pipeline pipeline) noexcept;

    void on_destroy_pipeline(
        reshade::api::device *device,
        reshade::api::pipeline pipeline) noexcept;

    bool pipeline_attested(
        std::uint64_t pipeline_handle) const noexcept;

    bool on_bind_pipeline(
        reshade::api::pipeline_stage stages,
        reshade::api::pipeline pipeline,
        std::uint16_t *first_bind_plan_index = nullptr,
        core::operator_mask *selected_owners = nullptr,
        std::uint16_t *selected_ops = nullptr,
        std::uint32_t *receiver_id = nullptr) noexcept;

    a1_runtime_telemetry telemetry() const noexcept;

    void reset() noexcept;

private:
    struct cache_key {
        std::uint16_t plan_index = 0;
        core::operator_mask owners = 0;

        bool operator==(
            const cache_key &other) const noexcept
        {
            return
                plan_index == other.plan_index &&
                owners == other.owners;
        }
    };

    struct cache_key_hash {
        std::size_t operator()(
            const cache_key &key) const noexcept
        {
            const std::uint64_t packed =
                static_cast<std::uint64_t>(
                    key.plan_index) |
                (static_cast<std::uint64_t>(
                    key.owners) << 16u);

            return static_cast<std::size_t>(
                packed ^ (packed >> 33u));
        }
    };

    struct replacement_record {
        std::shared_ptr<
            const std::vector<std::uint8_t>> bytes;

        operators::legacy_plan::hashing::
            sha256_digest output_sha256{};

        std::uint16_t plan_index = 0;
        std::uint16_t selected_ops = 0;
        core::operator_mask selected_owners = 0;
        bool full_plan_materialized = false;
        std::uint32_t receiver_id = 0;
    };

    bool accepts_device(
        reshade::api::device *device) noexcept;

    std::shared_ptr<
        const replacement_record>
    cache_replacement(
        const operators::legacy_plan::
            a1_create_time_outcome &outcome,
        std::vector<std::uint8_t> replacement);

    std::shared_ptr<
        const replacement_record>
    cache_replacement_record(
        std::uint16_t plan_index,
        core::operator_mask selected_owners,
        std::uint16_t selected_ops,
        bool full_plan_materialized,
        std::uint32_t receiver_id,
        const operators::legacy_plan::hashing::
            sha256_digest &output_sha256,
        std::vector<std::uint8_t> replacement);

    static reshade::api::shader_desc *
    find_mutable_pixel_shader(
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *
            subobjects) noexcept;

    static const reshade::api::shader_desc *
    find_pixel_shader(
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *
            subobjects) noexcept;

    static std::uint16_t plan_index_of(
        const operators::legacy_plan::generated::
            a1_exact_patch_plan *plan) noexcept;

    core::feature_registry &features_;

    mutable std::mutex mutex_;

    std::unordered_map<
        cache_key,
        std::shared_ptr<const replacement_record>,
        cache_key_hash> cache_;

    std::unordered_map<
        const void *,
        std::shared_ptr<const replacement_record>>
        code_records_;

    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const replacement_record>>
        pipeline_records_;

    reshade::api::device *device_ = nullptr;

    static constexpr std::size_t k_total_exact_plans = 168u;

    std::array<std::atomic_bool, k_total_exact_plans>
        first_bind_seen_{};

    std::atomic_bool quarantined_{false};

    std::atomic<std::uint64_t> create_events_{0};
    std::atomic<std::uint64_t> candidate_size_hits_{0};
    std::atomic<std::uint64_t> exact_identity_hits_{0};
    std::atomic<std::uint64_t> materialized_{0};
    std::atomic<std::uint64_t> pass_unknown_identity_{0};
    std::atomic<std::uint64_t> pass_no_enabled_owner_{0};
    std::atomic<std::uint64_t> fail_open_{0};
    std::atomic<std::uint64_t> init_attested_{0};
    std::atomic<std::uint64_t> init_mismatch_{0};
    std::atomic<std::uint64_t> target_binds_{0};
    std::atomic<std::uint64_t> build151_nospc_exact_hits_{0};
    std::atomic<std::uint64_t> build151_nospc_materialized_{0};
    std::atomic<std::uint64_t> build151_nospc_binds_{0};
};

} // namespace dsrrl::runtime
