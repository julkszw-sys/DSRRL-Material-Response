#pragma once

#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"

#include <reshade.hpp>

#include <cstdint>
#include <array>

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;

namespace dsrrl::runtime {

struct envspec_resource_telemetry {
    std::uint64_t native_candidates = 0;
    std::uint64_t native_matches = 0;
    std::uint64_t native_hash_miss = 0;
    std::uint64_t view_matches = 0;
    std::uint64_t pack_admit_ok = 0;
    std::uint64_t pack_admit_fail = 0;
    std::uint64_t cube_created = 0;
    std::uint64_t cube_fail = 0;
    std::uint64_t prepare_ok = 0;
    std::uint64_t prepare_fail = 0;
    bool pack_ready = false;
    bool sampler_ready = false;
};

struct prepared_envspec_resources {
    ID3D11ShaderResourceView *ptde_a = nullptr;
    ID3D11ShaderResourceView *ptde_b = nullptr;
    ID3D11SamplerState *sampler = nullptr;

    std::uint16_t probe_a = 0;
    std::uint16_t probe_b = 0;
    std::uint8_t slot = 0;
    bool probe_b_required = false;
    bool ready = false;
};

struct prepared_envdiffuse_resources {
    ID3D11ShaderResourceView *ptde_a = nullptr;
    ID3D11ShaderResourceView *ptde_b = nullptr;
    ID3D11SamplerState *sampler = nullptr;

    std::uint16_t probe_a = 0;
    std::uint16_t probe_b = 0;
    bool probe_b_required = false;
    bool ready = false;
};

class envspec_resource_runtime {
public:
    envspec_resource_runtime() noexcept = default;
    ~envspec_resource_runtime();

    envspec_resource_runtime(
        const envspec_resource_runtime &) = delete;
    envspec_resource_runtime &operator=(
        const envspec_resource_runtime &) = delete;

    bool register_events() noexcept;
    void unregister_events() noexcept;

    bool prepare(
        ID3D11DeviceContext *context,
        std::uint8_t slot,
        bool probe_b_required,
        prepared_envspec_resources &prepared) noexcept;

    // Runtime v2 fast path. Consumes already-tracked stock SRV bindings and
    // avoids D3D11 PSGetShaderResources on every qualifying draw.
    bool prepare_bound(
        ID3D11ShaderResourceView *stock_a,
        ID3D11ShaderResourceView *stock_b,
        std::uint8_t slot,
        bool probe_b_required,
        prepared_envspec_resources &prepared) noexcept;

    // Diagnostic resource falsifier: preserve the exact PTDE EnvSpec shader,
    // A/B LightBank feed and PTDE sampler while feeding the currently bound,
    // exact-identity DSR BC6H probe SRVs instead of materialized PackedGI.
    // This isolates resource content/encoding/mapping from the consumer
    // equation. Unknown native probe identity remains fail-open.
    bool prepare_native_dsr(
        ID3D11DeviceContext *context,
        std::uint8_t slot,
        bool probe_b_required,
        prepared_envspec_resources &prepared) noexcept;

    // Full-PTDE HemEnv resource cut. Probe identity is supplied by the already
    // authenticated t12/t14 EnvSpec carrier; this method does not infer an
    // independent probe assignment from t11/t13.
    // EnvDiffuse's OWN source identity: stock t11/t13 native SHA + COM
    // lifetime, independent of EnvSpec t12/t14 probe ordinals or pack.
    // Only one-to-one authenticated DSR t11 -> PTDE EnvDiffuse probe
    // identity is admitted; absent/ambiguous host resources fail-open.
    bool prepare_envdiffuse_bound(
        ID3D11ShaderResourceView *stock_t11,
        ID3D11ShaderResourceView *stock_t13,
        bool probe_b_required,
        prepared_envdiffuse_resources &prepared) noexcept;

    bool prepare_envdiffuse(
        std::uint16_t probe_a,
        std::uint16_t probe_b,
        bool probe_b_required,
        prepared_envdiffuse_resources &prepared) noexcept;

    void release(
        prepared_envspec_resources &prepared) noexcept;
    void release(
        prepared_envdiffuse_resources &prepared) noexcept;

#if defined(DSRRL_V336_WANDERER_SRV_AUDIT)
    // The SHA256 of one already-verified PTDE PackedGI 32x32x6 RGBA8
    // resource slice. No GPU readback, shader edits, or resource replacement.
    bool audit_packed_cube_sha256(
        std::uint16_t probe, std::uint8_t slot,
        std::array<std::uint8_t,32> &digest) const noexcept;
#endif

    envspec_resource_telemetry telemetry() const noexcept;
    void reset_stats() noexcept;
};

} // namespace dsrrl::runtime
