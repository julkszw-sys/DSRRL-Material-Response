#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_materializer.hpp"
#include "dsrrl/operators/env_spec/pmetal_rgba_lerp_materializer.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"
#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"
#include "dsrrl/runtime/envspec_resource_runtime.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/material_resource_draw_runtime.hpp"
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"

#include <reshade.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11PixelShader;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace dsrrl::runtime {

enum class pmetal_envspec_receiver_family : std::uint8_t {
    stable_hemenv = 0,
    hemenvlerp
};

// Exact draw-local gate shared by the dedicated P_Metal EnvSpec runtime and
// the integrated dispatcher. This is intentionally stricter than route 345:
// a route hit alone must never allow a failed dedicated P_Metal island to
// fall through into generic diffuse-only Material Response.
bool exact_pmetal_envspec_candidate(
    const operators::material_response::material_identity &material,
    const operators::material_response::decision &decision) noexcept;

struct prepared_pmetal_envspec_draw {
    island_draw_adapter_request request{};
    prepared_envspec_resources env_resources{};
    prepared_envdiffuse_resources envdiffuse_resources{};
    prepared_material_resource_draw material_resources{};
    ID3D11PixelShader *shader = nullptr;
    ID3D11Buffer *b12 = nullptr;
    // R7 exact PTDE shadow sampler is AddRef-held for the replay lifetime.
    ID3D11SamplerState *shadow_sampler = nullptr;

    // Diagnostic-only native DSR EnvDiffuse ownership. PSGetShaderResources
    // AddRefs these stock host SRVs; keep them alive until replay completes.
    ID3D11ShaderResourceView *native_dsr_envdiffuse_a = nullptr;
    ID3D11ShaderResourceView *native_dsr_envdiffuse_b = nullptr;

#if defined(DSRRL_V291_LERP_CUBEMAP_CENSUS)
    bool diagnostic_lerp = false;
    float diagnostic_beta = 0.0f;
    std::uint64_t diagnostic_serial = 0u;
    std::uint16_t diagnostic_probe_a = 0u, diagnostic_probe_b = 0u;
    std::uint16_t diagnostic_diff_a = 0u, diagnostic_diff_b = 0u;
#endif
    std::uint8_t source_kind = 0u; // diagnostic only
    // V388 STABLE diagnostic: read-only source identity propagated from
    // request_ready to indexed_issued. Never used for eligibility/patching.
    bool stable_envdiffuse_trace_valid = false;
    bool stable_envdiffuse_independent = false;
    std::uint64_t stable_envdiffuse_source_serial = 0u;
    std::uint64_t stable_envdiffuse_bank = 0u;
    std::uint32_t stable_envdiffuse_row = 0u;
    std::array<float,3> stable_envdiffuse_c86{};
    std::uint16_t stable_envdiffuse_probe = 0u;
    std::array<std::uint32_t,4> v389_tail_bits{};
    bool v389_tail_valid = false;
    bool v389_cb_reused = false;

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
    std::uint64_t requests = 0;
    std::uint64_t lerp_requests = 0;
    std::uint64_t b12_uploads = 0;
    std::uint64_t b12_reuses = 0;
    std::uint64_t srv_shadow_hits = 0;
    std::uint64_t srv_shadow_fallbacks = 0;

    // Low-overhead effect ladder. These are one-way session latches used by
    // DSRRL_EFFECT_TELEMETRY and do not imply pixel equivalence.
    bool effect_entry_seen = false;
    bool effect_feature_ready = false;
    bool effect_material_ready = false;
    bool effect_semantic_ready = false;
    bool effect_source_ready = false;
    bool effect_receiver_source_ready = false;
    bool effect_replacement_ready = false;
    bool effect_probe_ready = false;
    bool effect_spec_rgb_ready = false;
    bool effect_b12_ready = false;
    bool effect_request_ready = false;
    std::uint32_t effect_fail_mask = 0u;

    bool quarantined = false;
};

class pmetal_envspec_draw_runtime {
public:
    pmetal_envspec_draw_runtime(
        core::renderer_core &core,
        pmetal_env_source_runtime &source,
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

#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
    // Alternative shader only. P_Metal's registered PS is immutable.
    bool register_lerp_nonmetal_r8_candidate(
        const operators::env_spec::
            pmetal_rgba_lerp_materialize_outcome &outcome,
        const void *r19_dxbc,
        std::size_t r19_size) noexcept;
#endif

#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
    bool register_independent_envdiffuse(
        const void *dxbc,std::size_t size,
        std::uint32_t receiver_id) noexcept;

    // Exact PTDE EnvDiffuse isolated from EnvSpec. Requires verified
    // independent native t11 -> PTDE cubemap, LightBank c86, shader and CB12.
    bool prepare_independent_envdiffuse(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        const operators::material_response::decision &decision,
        prepared_pmetal_envspec_draw &prepared) noexcept;
#endif

    bool prepare(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        const operators::material_response::decision &decision,
        prepared_pmetal_envspec_draw &prepared) noexcept;

    bool prepare(
        reshade::api::command_list *cmd_list,
        const operators::material_response::material_identity &material,
        const operators::material_response::decision &decision,
        pmetal_envspec_receiver_family family,
        prepared_pmetal_envspec_draw &prepared) noexcept;

    void release(
        prepared_pmetal_envspec_draw &prepared) noexcept;

    pmetal_envspec_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct replacement_record {
        ID3D11PixelShader *shader = nullptr;
        core::operator_mask composed_owners = 0;
        core::sha256_digest payload_sha256{};
        std::size_t payload_size = 0u;
    };

    struct b12_context_cache {
        ID3D11Buffer *buffer = nullptr;
#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
        std::array<std::uint32_t,24> payload_bits{};
#else
        std::array<std::uint32_t,16> payload_bits{};
#endif
        bool payload_valid = false;
    };

    void release_resources() noexcept;

    core::renderer_core &core_;
    pmetal_env_source_runtime &source_;
    envspec_resource_runtime &env_resources_;
    material_resource_draw_runtime &material_resources_;

    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    // Device-owned exact D3D11 equivalent of the PTDE stage-7 point/border
    // shadow sampler. Only receiver 33/34 R7 draws may borrow it.
    ID3D11SamplerState *shadow_sampler_ = nullptr;
    std::unordered_map<std::uint32_t,replacement_record>
        replacements_;
    std::unordered_map<std::uint32_t,ID3D11PixelShader *>
        lerp_replacements_;
#if defined(DSRRL_V289_NONMETAL_LERP_R8_SPEC_LINEAR_AB)
    std::unordered_map<std::uint32_t,ID3D11PixelShader *>
        lerp_nonmetal_r8_replacements_;
#endif
    std::unordered_map<std::uint32_t,core::sha256_digest>
        lerp_replacement_sha256_;
    std::unordered_map<std::uint32_t,std::size_t>
        lerp_replacement_size_;
    std::unordered_map<std::uintptr_t,b12_context_cache>
        b12_by_context_;
#if defined(DSRRL_V370_ISOLATED_EXACT_ENVDIFFUSE)
    std::unordered_map<std::uint32_t,ID3D11PixelShader *>
        independent_envdiffuse_shaders_;
    struct envdiffuse_cb_cache {
        ID3D11Buffer *buffer = nullptr;
        std::array<std::uint32_t,16> payload_bits{};
        bool valid = false;
    };
    std::unordered_map<std::uintptr_t,envdiffuse_cb_cache>
        independent_envdiffuse_cb_by_context_;
#endif

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
    std::atomic<std::uint64_t> requests_{0};
    std::atomic<std::uint64_t> lerp_requests_{0};
    std::atomic<std::uint64_t> b12_uploads_{0};
    std::atomic<std::uint64_t> b12_reuses_{0};
    std::atomic<std::uint64_t> srv_shadow_hits_{0};
    std::atomic<std::uint64_t> srv_shadow_fallbacks_{0};

    std::atomic_bool effect_entry_seen_{false};
    std::atomic_bool effect_feature_ready_{false};
    std::atomic_bool effect_material_ready_{false};
    std::atomic_bool effect_semantic_ready_{false};
    std::atomic_bool effect_source_ready_{false};
    std::atomic_bool effect_receiver_source_ready_{false};
    std::atomic_bool effect_replacement_ready_{false};
    std::atomic_bool effect_probe_ready_{false};
    std::atomic_bool effect_spec_rgb_ready_{false};
    std::atomic_bool effect_b12_ready_{false};
    std::atomic_bool effect_request_ready_{false};
    std::atomic<std::uint32_t> effect_fail_mask_{0u};

    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
