#pragma once

#include "dsrrl/core/renderer_core.hpp"
#include "dsrrl/runtime/island_draw_adapter.hpp"
#include "dsrrl/runtime/upper_lower_draw_runtime.hpp"
#include "dsrrl/runtime/upper_lower_pipeline_registry.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11PixelShader;

namespace dsrrl::runtime {

struct prepared_upper_lower_hemenv_draw {
    island_draw_adapter_request request{};
    prepared_upper_lower_draw carrier{};
    ID3D11PixelShader *shader = nullptr;
    upper_lower_receiver_identity identity{};
    bool ready = false;
};

struct upper_lower_hemenv_draw_telemetry {
    std::uint64_t replacement_register_ok = 0;
    std::uint64_t replacement_register_fail = 0;
    std::uint64_t candidates = 0;
    std::uint64_t carrier_ready = 0;
    std::uint64_t carrier_rejects = 0;
    std::uint64_t identity_rejects = 0;
    std::uint64_t nospc_ready = 0;
    std::uint64_t spc_ready = 0;
    std::uint64_t spc_mr_hold = 0;
    std::uint64_t phn_ready = 0;
    std::uint64_t gst_ready = 0;
    std::uint64_t sfx_ready = 0;
    std::uint64_t snow_ready = 0;
    std::uint64_t ntoa_ready = 0;
    std::uint64_t requests = 0;
    bool quarantined = false;
};

class upper_lower_hemenv_draw_runtime {
public:
    upper_lower_hemenv_draw_runtime(
        core::renderer_core &core,
        upper_lower_draw_runtime &lightbank) noexcept;
    ~upper_lower_hemenv_draw_runtime();

    upper_lower_hemenv_draw_runtime(
        const upper_lower_hemenv_draw_runtime &) = delete;
    upper_lower_hemenv_draw_runtime &operator=(
        const upper_lower_hemenv_draw_runtime &) = delete;

    void on_init_device(
        reshade::api::device *device) noexcept;
    void on_destroy_device(
        reshade::api::device *device) noexcept;

    bool register_replacement(
        const operators::lightbank::
            upper_lower_hemenv_materialize_outcome &outcome,
        const void *dxbc,
        std::size_t dxbc_size) noexcept;

    bool prepare_draw_request(
        reshade::api::command_list *cmd_list,
        const upper_lower_receiver_identity &identity,
        bool material_response_active,
        prepared_upper_lower_hemenv_draw &prepared) noexcept;

    void release_prepared_draw(
        prepared_upper_lower_hemenv_draw &prepared) noexcept;

    upper_lower_hemenv_draw_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    struct replacement_record {
        ID3D11PixelShader *shader = nullptr;
        operators::lightbank::upper_lower_hemenv_stratum stratum =
            operators::lightbank::upper_lower_hemenv_stratum::nospc;
        operators::lightbank::upper_lower_hemenv_family family =
            operators::lightbank::upper_lower_hemenv_family::hemenv;
        std::uint8_t stable_receiver_id = 0u;
        core::operator_mask composed_owners = 0u;
    };

    void release_resources() noexcept;

    core::renderer_core &core_;
    upper_lower_draw_runtime &lightbank_;

    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::uint16_t,replacement_record>
        replacements_;

    std::atomic<std::uint64_t> replacement_register_ok_{0};
    std::atomic<std::uint64_t> replacement_register_fail_{0};
    std::atomic<std::uint64_t> candidates_{0};
    std::atomic<std::uint64_t> carrier_ready_{0};
    std::atomic<std::uint64_t> carrier_rejects_{0};
    std::atomic<std::uint64_t> identity_rejects_{0};
    std::atomic<std::uint64_t> nospc_ready_{0};
    std::atomic<std::uint64_t> spc_ready_{0};
    std::atomic<std::uint64_t> spc_mr_hold_{0};
    std::atomic<std::uint64_t> phn_ready_{0};
    std::atomic<std::uint64_t> gst_ready_{0};
    std::atomic<std::uint64_t> sfx_ready_{0};
    std::atomic<std::uint64_t> snow_ready_{0};
    std::atomic<std::uint64_t> ntoa_ready_{0};
    std::atomic<std::uint64_t> requests_{0};
    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
