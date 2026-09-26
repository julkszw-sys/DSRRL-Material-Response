#pragma once

#include "dsrrl/core/renderer_core.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL generic draw transaction requires ReShade Add-on API 20
#endif

#include <array>
#include <atomic>
#include <cstdint>

struct ID3D11Buffer;
struct ID3D11PixelShader;
struct ID3D11SamplerState;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime {

inline constexpr std::size_t draw_tx_max_cb = 8u;
inline constexpr std::size_t draw_tx_max_srv = 16u;
inline constexpr std::size_t draw_tx_max_sampler = 8u;
inline constexpr std::size_t draw_tx_max_class_instances = 256u;

struct draw_tx_cb_binding {
    std::uint32_t slot = 0;
    ID3D11Buffer *buffer = nullptr;

    // Exact semantic owners of this CB slot. Zero is accepted at request
    // construction time as shorthand for the primary island only; composed
    // requests must assign explicit owners for shared slots.
    core::operator_mask owners = 0;
};

struct draw_tx_srv_binding {
    std::uint32_t slot = 0;
    ID3D11ShaderResourceView *srv = nullptr;
};

struct draw_tx_sampler_binding {
    std::uint32_t slot = 0;
    ID3D11SamplerState *sampler = nullptr;
};

struct draw_tx_mutation {
    // Aggregate participation plus operator-local ownership of each mutation
    // class. An owner never inherits another island's shader/resource/carrier
    // mutation merely because both are composed into one replay.
    core::operator_mask owners = 0;
    core::operator_mask shader_owners = 0;
    core::operator_mask constant_buffer_owners = 0;
    core::operator_mask resource_owners = 0;

    // b13 lane-level ownership only. Ordinary CB ownership (e.g. shared b12)
    // is tracked independently in constant_buffer_owners.
    core::operator_mask carrier_owners = 0;

    ID3D11PixelShader *pixel_shader = nullptr;
    bool replace_pixel_shader = false;

    std::array<draw_tx_cb_binding, draw_tx_max_cb> constant_buffers{};
    std::uint32_t constant_buffer_count = 0;

    std::array<draw_tx_srv_binding, draw_tx_max_srv> srvs{};
    std::uint32_t srv_count = 0;

    std::array<draw_tx_sampler_binding, draw_tx_max_sampler> samplers{};
    std::uint32_t sampler_count = 0;
};

enum class draw_tx_result : std::uint8_t {
    not_issued = 0,
    issued_restored,
    issued_restore_failed
};

struct draw_tx_telemetry {
    std::uint64_t begin_ok = 0;
    std::uint64_t begin_fail = 0;
    std::uint64_t bind_fail = 0;
    std::uint64_t draws_issued = 0;
    std::uint64_t restore_ok = 0;
    std::uint64_t restore_fail = 0;
    bool quarantined = false;
};

class draw_state_transaction_runtime {
public:
    explicit draw_state_transaction_runtime(core::renderer_core &core) noexcept;

    draw_state_transaction_runtime(
        const draw_state_transaction_runtime &) = delete;
    draw_state_transaction_runtime &operator=(
        const draw_state_transaction_runtime &) = delete;

    draw_tx_result replay_draw(
        reshade::api::command_list *cmd_list,
        const draw_tx_mutation &mutation,
        std::uint32_t vertex_count,
        std::uint32_t instance_count,
        std::uint32_t first_vertex,
        std::uint32_t first_instance) noexcept;

    draw_tx_result replay_draw_indexed(
        reshade::api::command_list *cmd_list,
        const draw_tx_mutation &mutation,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    draw_tx_telemetry telemetry() const noexcept;
    bool quarantined() const noexcept;
    void reset() noexcept;

private:
    struct cb_capture {
        std::uint32_t slot = 0;
        ID3D11Buffer *base = nullptr;
        ID3D11Buffer *window = nullptr;
        std::uint32_t first = 0;
        std::uint32_t count = 0;
        bool explicit_window = false;
        bool coherent = true;
    };

    struct srv_capture {
        std::uint32_t slot = 0;
        ID3D11ShaderResourceView *srv = nullptr;
    };

    struct sampler_capture {
        std::uint32_t slot = 0;
        ID3D11SamplerState *sampler = nullptr;
    };

    struct transaction_state {
        ID3D11PixelShader *old_shader = nullptr;
        std::array<void *, draw_tx_max_class_instances> old_classes{};
        std::uint32_t old_class_count = 0;

        std::array<cb_capture, draw_tx_max_cb> cbs{};
        std::uint32_t cb_count = 0;

        std::array<srv_capture, draw_tx_max_srv> srvs{};
        std::uint32_t srv_count = 0;

        std::array<sampler_capture, draw_tx_max_sampler> samplers{};
        std::uint32_t sampler_count = 0;

        std::uint64_t command = 0;
        bool core_started = false;
    };

    bool validate_mutation(
        const draw_tx_mutation &mutation) const noexcept;

    bool begin(
        reshade::api::command_list *cmd_list,
        const draw_tx_mutation &mutation,
        transaction_state &state) noexcept;

    bool restore(
        reshade::api::command_list *cmd_list,
        transaction_state &state) noexcept;

    void release_state(transaction_state &state) noexcept;

    core::renderer_core &core_;
    std::atomic<std::uint64_t> draw_serial_{0};
    std::atomic<std::uint64_t> begin_ok_{0};
    std::atomic<std::uint64_t> begin_fail_{0};
    std::atomic<std::uint64_t> bind_fail_{0};
    std::atomic<std::uint64_t> draws_issued_{0};
    std::atomic<std::uint64_t> restore_ok_{0};
    std::atomic<std::uint64_t> restore_fail_{0};
    std::atomic_bool quarantined_{false};
};

constexpr bool draw_tx_issued(draw_tx_result result) noexcept
{
    return result != draw_tx_result::not_issued;
}

} // namespace dsrrl::runtime
