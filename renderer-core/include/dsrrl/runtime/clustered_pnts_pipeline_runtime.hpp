#pragma once

#include "dsrrl/operators/point_light/clustered_pnts_direct_materializer.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL clustered PntS pipeline runtime requires ReShade Add-on API 20
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

struct ID3D11PixelShader;

namespace dsrrl::runtime {

struct clustered_pnts_pipeline_telemetry {
    std::uint64_t candidates = 0;
    std::uint64_t candidate_create_ok = 0;
    std::uint64_t candidate_create_fail = 0;
    std::uint64_t init_attested = 0;
    std::uint64_t init_miss = 0;
    std::uint64_t bind_hits = 0;
    std::uint64_t bind_misses = 0;
    bool quarantined = false;
};

struct prepared_clustered_pnts_shader {
    ID3D11PixelShader *shader = nullptr;
    bool spc = false;
    bool blended_material = false;
    std::uint32_t representative_shader_index = 0u;
    core::operator_mask composed_shader_owners = 0u;
    bool current_b12_abi = false;
    bool legacy_specular_complete = false;
    bool ready = false;
};

class clustered_pnts_pipeline_runtime {
public:
    clustered_pnts_pipeline_runtime() noexcept = default;
    ~clustered_pnts_pipeline_runtime();

    clustered_pnts_pipeline_runtime(
        const clustered_pnts_pipeline_runtime &) = delete;
    clustered_pnts_pipeline_runtime &operator=(
        const clustered_pnts_pipeline_runtime &) = delete;

    // "attested_host" is the exact PS bytecode after every earlier create-time
    // bridge (notably A1) has run. The PTDE replacement itself is still
    // materialized from the exact original stock body.
    bool register_candidate(
        reshade::api::device *device,
        const operators::point_light::
            clustered_pnts_direct_materialize_outcome &outcome,
        const std::uint8_t *attested_host,
        std::size_t attested_host_size,
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

    bool bound_metadata(
        reshade::api::command_list *cmd_list,
        bool &spc,
        bool &blended_material) const noexcept;

    bool prepare_bound_shader(
        reshade::api::command_list *cmd_list,
        prepared_clustered_pnts_shader &prepared) noexcept;

    void release_prepared_shader(
        prepared_clustered_pnts_shader &prepared) noexcept;

    clustered_pnts_pipeline_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct digest_key {
        std::array<std::uint8_t,32> sha{};
        std::size_t size = 0u;
        bool operator==(const digest_key &other) const noexcept
        {
            return size == other.size && sha == other.sha;
        }
    };

    struct digest_hash {
        std::size_t operator()(
            const digest_key &key) const noexcept;
    };

    struct record;

    struct bound_tls_state {
        const clustered_pnts_pipeline_runtime *runtime = nullptr;
        std::uint64_t command = 0u;
        std::shared_ptr<const record> selected{};
        std::uint64_t epoch = 0u;
        bool present = false;
        std::uint64_t pipeline = 0u;
        std::uint64_t pipeline_epoch = 0u;
    };

    struct attestation_tls_entry {
        const clustered_pnts_pipeline_runtime *runtime = nullptr;
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
    reshade::api::device *device_ = nullptr;
    std::atomic<std::uint64_t> pipeline_epoch_{1u};

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
