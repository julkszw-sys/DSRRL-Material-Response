#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/hemdir3_pipeline_registry.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/operators/material_response/material_response_island.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <unordered_map>

struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11PixelShader;

namespace dsrrl::runtime {

struct prepared_hemdir3_draw {
    island_draw_adapter_request request{};
    prepared_hemdir3_carrier carrier{};
    ID3D11Buffer *b12 = nullptr;
    ID3D11PixelShader *shader = nullptr;
    hemdir3_receiver_identity identity{};
    bool ready = false;
};

struct hemdir3_draw_telemetry {
    std::uint64_t replacement_register_ok = 0;
    std::uint64_t replacement_register_fail = 0;
    std::uint64_t candidates = 0;
    std::uint64_t mode2_hits = 0;
    std::uint64_t mode_rejects = 0;
    std::uint64_t carrier_ready = 0;
    std::uint64_t carrier_rejects = 0;
    std::uint64_t nospc_ready = 0;
    std::uint64_t spc_ready = 0;
    std::uint64_t spc_donor_hit = 0;
    std::uint64_t spc_donor_miss = 0;
    std::uint64_t spc_b12_create = 0;
    std::uint64_t spc_b12_hit = 0;
    std::uint64_t spc_b12_hold = 0;
    std::uint64_t readiness_rejects = 0;
    std::uint64_t requests = 0;
    bool quarantined = false;
};

class hemdir3_draw_runtime {
public:
    hemdir3_draw_runtime(
        core::renderer_core &core,
        upper_lower_draw_runtime &lightbank) noexcept;
    ~hemdir3_draw_runtime();

    hemdir3_draw_runtime(
        const hemdir3_draw_runtime &) = delete;
    hemdir3_draw_runtime &operator=(
        const hemdir3_draw_runtime &) = delete;

    void on_init_device(
        reshade::api::device *device) noexcept;
    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool register_replacement(
        const operators::lightbank::
            hemdir3_b13_materialize_outcome &outcome,
        const void *dxbc,
        std::size_t dxbc_size) noexcept;

    bool prepare_draw_request(
        reshade::api::command_list *cmd_list,
        const hemdir3_receiver_identity &identity,
        const operators::material_response::material_identity &material,
        prepared_hemdir3_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_hemdir3_draw &prepared) noexcept;

    hemdir3_draw_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct replacement_record {
        ID3D11PixelShader *shader = nullptr;
        operators::lightbank::hemdir3_native_stratum stratum =
            operators::lightbank::hemdir3_native_stratum::nospc;
        std::uint8_t paired_stable_receiver_id = 0u;
        core::operator_mask composed_owners = 0u;
    };

    ID3D11Buffer *realize_spc_b12(
        const operators::material_response::material_identity &material) noexcept;
    void release_resources() noexcept;

    core::renderer_core &core_;
    upper_lower_draw_runtime &lightbank_;

    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::uint16_t,replacement_record>
        replacements_;
    std::map<core::sha256_digest,ID3D11Buffer *>
        spc_b12_by_mtd_;

    std::atomic<std::uint64_t> replacement_register_ok_{0};
    std::atomic<std::uint64_t> replacement_register_fail_{0};
    std::atomic<std::uint64_t> candidates_{0};
    std::atomic<std::uint64_t> mode2_hits_{0};
    std::atomic<std::uint64_t> mode_rejects_{0};
    std::atomic<std::uint64_t> carrier_ready_{0};
    std::atomic<std::uint64_t> carrier_rejects_{0};
    std::atomic<std::uint64_t> nospc_ready_{0};
    std::atomic<std::uint64_t> spc_ready_{0};
    std::atomic<std::uint64_t> spc_donor_hit_{0};
    std::atomic<std::uint64_t> spc_donor_miss_{0};
    std::atomic<std::uint64_t> spc_b12_create_{0};
    std::atomic<std::uint64_t> spc_b12_hit_{0};
    std::atomic<std::uint64_t> spc_b12_hold_{0};
    std::atomic<std::uint64_t> readiness_rejects_{0};
    std::atomic<std::uint64_t> requests_{0};
    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
