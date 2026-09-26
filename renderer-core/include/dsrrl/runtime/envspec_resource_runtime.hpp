#pragma once

#include "dsrrl/operators/env_spec/legacy_resource_bridge.hpp"

#include <reshade.hpp>

#include <cstdint>

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

    void release(
        prepared_envspec_resources &prepared) noexcept;

    envspec_resource_telemetry telemetry() const noexcept;
    void reset_stats() noexcept;
};

} // namespace dsrrl::runtime
