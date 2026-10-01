#pragma once

#include "dsrrl/operators/point_light/fixed_local_specular_single_materializer.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL fixed PointLight pipeline runtime requires ReShade Add-on API 20
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

struct ID3D11PixelShader;

namespace dsrrl::runtime {

struct fixed_pointlight_pipeline_telemetry {
    std::uint64_t candidates = 0;
    std::uint64_t candidate_create_ok = 0;
    std::uint64_t candidate_create_fail = 0;
    std::uint64_t init_attested = 0;
    std::uint64_t init_miss = 0;
    std::uint64_t bind_hits = 0;
    std::uint64_t bind_misses = 0;
    bool quarantined = false;
};

struct prepared_fixed_pointlight_shader {
    ID3D11PixelShader *shader = nullptr;
    std::uint8_t light_count = 0u;
    bool blended_material = false;
    bool ready = false;
};

class fixed_pointlight_pipeline_runtime {
public:
    fixed_pointlight_pipeline_runtime() noexcept = default;
    ~fixed_pointlight_pipeline_runtime();

    fixed_pointlight_pipeline_runtime(
        const fixed_pointlight_pipeline_runtime &) = delete;
    fixed_pointlight_pipeline_runtime &operator=(
        const fixed_pointlight_pipeline_runtime &) = delete;

    bool register_candidate(
        reshade::api::device *device,
        const operators::point_light::
            fixed_local_single_materialize_outcome &outcome,
        const std::uint8_t *replacement,
        std::size_t replacement_size) noexcept;

    bool on_init_pipeline(
        reshade::api::device *device,
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects,
        reshade::api::pipeline pipeline) noexcept;

    bool on_bind_pipeline(
        reshade::api::command_list *cmd_list,
        reshade::api::pipeline_stage stages,
        reshade::api::pipeline pipeline) noexcept;

    void on_destroy_pipeline(
        reshade::api::pipeline pipeline) noexcept;

    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool pipeline_attested(
        std::uint64_t pipeline_handle) const noexcept;

    // Bind-time exact registry lookup with a small epoch-invalidated TLS
    // cache. This is the authoritative recovery path when the integrated
    // route cache has lost the PointLight bit.
    bool pipeline_attested_cached(
        std::uint64_t pipeline_handle) const noexcept;

    bool bound_light_count(
        reshade::api::command_list *cmd_list,
        std::uint8_t &light_count) const noexcept;

    bool prepare_bound_shader(
        reshade::api::command_list *cmd_list,
        prepared_fixed_pointlight_shader &prepared) noexcept;

    void release_prepared_shader(
        prepared_fixed_pointlight_shader &prepared) noexcept;

    fixed_pointlight_pipeline_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct digest_key {
        std::array<std::uint8_t,32> sha{};
        std::size_t size = 0u;
        bool operator==(const digest_key &other) const noexcept
        {
            return size==other.size && sha==other.sha;
        }
    };

    struct digest_hash {
        std::size_t operator()(const digest_key &key) const noexcept;
    };

    struct record;

    struct bound_tls_state {
        const fixed_pointlight_pipeline_runtime *runtime = nullptr;
        std::uint64_t command = 0u;
        std::shared_ptr<const record> selected{};
        std::uint64_t epoch = 0u;
        bool present = false;
        std::uint64_t pipeline = 0u;
        std::uint64_t pipeline_epoch = 0u;
    };

    struct attestation_tls_entry {
        const fixed_pointlight_pipeline_runtime *runtime = nullptr;
        std::uint64_t pipeline = 0u;
        std::uint64_t epoch = 0u;
        std::shared_ptr<const record> selected{};
        bool present = false;
    };

    static constexpr std::size_t k_attestation_cache_size = 64u;
    static thread_local bound_tls_state bound_tls_;
    static thread_local std::array<
        attestation_tls_entry,
        k_attestation_cache_size> attestation_tls_;

    static const reshade::api::shader_desc *find_pixel_shader(
        std::uint32_t subobject_count,
        const reshade::api::pipeline_subobject *subobjects) noexcept;

    std::shared_ptr<const record> pipeline_record_cached(
        std::uint64_t pipeline_handle) const noexcept;

    mutable std::mutex mutex_;
    std::unordered_map<
        digest_key,
        std::shared_ptr<const record>,
        digest_hash> candidates_;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const record>> pipelines_;
    std::unordered_map<
        std::uint64_t,
        std::shared_ptr<const record>> bound_;
    reshade::api::device *device_ = nullptr;
    std::atomic<std::uint64_t> bound_epoch_{1u};
    std::atomic<std::uint64_t> pipeline_epoch_{1u};
    std::atomic_bool any_bound_{false};

    std::atomic<std::uint64_t> candidates_seen_{0};
    std::atomic<std::uint64_t> candidate_create_ok_{0};
    std::atomic<std::uint64_t> candidate_create_fail_{0};
    std::atomic<std::uint64_t> init_attested_{0};
    std::atomic<std::uint64_t> init_miss_{0};
    std::atomic<std::uint64_t> bind_hits_{0};
    std::atomic<std::uint64_t> bind_misses_{0};
    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
