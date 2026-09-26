#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/runtime/envspec_resource_runtime.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11PixelShader;

namespace dsrrl::runtime {

enum class pmetal_envspec_receiver_family : std::uint8_t {
    stable_hemenv = 0,
    hemenvlerp
};

struct prepared_pmetal_envspec_draw {
    island_draw_adapter_request request{};
    prepared_envspec_resources env_resources{};
    prepared_material_resource_draw material_resources{};
    prepared_upper_lower_draw upper_lower{};

    ID3D11PixelShader *shader = nullptr;
    ID3D11Buffer *b12 = nullptr;

    bool upper_lower_composed = false;
    bool ready = false;
};

struct pmetal_envspec_telemetry {
    std::uint64_t replacement_register_ok = 0;
    std::uint64_t replacement_register_fail = 0;
    std::uint64_t lerp_replacement_register_ok = 0;
    std::uint64_t lerp_replacement_register_fail = 0;
    std::uint64_t candidates = 0;
    std::uint64_t lerp_candidates = 0;
    std::uint64_t material_rejects = 0;
    std::uint64_t semantic_rejects = 0;
    std::uint64_t source_rejects = 0;
    std::uint64_t blended_receiver_hold = 0;
    std::uint64_t probe_rejects = 0;
    std::uint64_t spec_rgb_rejects = 0;
    std::uint64_t upper_lower_ready = 0;
    std::uint64_t upper_lower_fallback = 0;
    std::uint64_t requests = 0;
    std::uint64_t lerp_requests = 0;
    bool quarantined = false;
};

class pmetal_envspec_draw_runtime {
public:
    pmetal_envspec_draw_runtime(
        core::renderer_core &core,
        upper_lower_draw_runtime &lightbank,
        envspec_resource_runtime &env_resources,
        material_resource_draw_runtime &material_resources) noexcept;

    ~pmetal_envspec_draw_runtime();

    void on_init_device(
        reshade::api::device *device) noexcept;
    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool register_replacement(
        const operators::env_spec::
            pmetal_rgba_materialize_outcome &outcome,
        const void *dxbc,
        std::size_t dxbc_size) noexcept;

    bool register_lerp_replacement(
        const operators::env_spec::
            pmetal_rgba_lerp_materialize_outcome &outcome,
        const void *dxbc,
        std::size_t dxbc_size) noexcept;

    bool prepare(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        const operators::material_response::decision &decision,
        bool upper_lower_receiver_verified,
        prepared_pmetal_envspec_draw &prepared) noexcept;

    bool prepare(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        const operators::material_response::decision &decision,
        pmetal_envspec_receiver_family family,
        bool upper_lower_receiver_verified,
        prepared_pmetal_envspec_draw &prepared) noexcept;

    void release(
        prepared_pmetal_envspec_draw &prepared) noexcept;

    pmetal_envspec_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct replacement_pair {
        ID3D11PixelShader *base = nullptr;
        ID3D11PixelShader *upper_lower = nullptr;
        core::operator_mask base_owners = 0;
        core::operator_mask upper_lower_owners = 0;
    };

    void release_resources() noexcept;

    core::renderer_core &core_;
    upper_lower_draw_runtime &lightbank_;
    envspec_resource_runtime &env_resources_;
    material_resource_draw_runtime &material_resources_;

    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::uint32_t,replacement_pair>
        replacements_;
    std::unordered_map<std::uint32_t,ID3D11PixelShader *>
        lerp_replacements_;
    std::unordered_map<std::uintptr_t,ID3D11Buffer *>
        b12_by_context_;

    std::atomic<std::uint64_t> replacement_register_ok_{0};
    std::atomic<std::uint64_t> replacement_register_fail_{0};
    std::atomic<std::uint64_t> lerp_replacement_register_ok_{0};
    std::atomic<std::uint64_t> lerp_replacement_register_fail_{0};
    std::atomic<std::uint64_t> candidates_{0};
    std::atomic<std::uint64_t> lerp_candidates_{0};
    std::atomic<std::uint64_t> material_rejects_{0};
    std::atomic<std::uint64_t> semantic_rejects_{0};
    std::atomic<std::uint64_t> source_rejects_{0};
    std::atomic<std::uint64_t> blended_receiver_hold_{0};
    std::atomic<std::uint64_t> probe_rejects_{0};
    std::atomic<std::uint64_t> spec_rgb_rejects_{0};
    std::atomic<std::uint64_t> upper_lower_ready_{0};
    std::atomic<std::uint64_t> upper_lower_fallback_{0};
    std::atomic<std::uint64_t> requests_{0};
    std::atomic<std::uint64_t> lerp_requests_{0};
    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
