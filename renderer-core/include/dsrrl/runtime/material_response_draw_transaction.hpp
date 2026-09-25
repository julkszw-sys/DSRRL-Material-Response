#pragma once

#include "dsrrl/core/renderer_core.hpp"

#include <reshade.hpp>

#if RESHADE_API_VERSION != 20
#error DSRRL Material Response draw transaction requires ReShade Add-on API 20
#endif

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

struct ID3D11Device;
struct ID3D11PixelShader;

namespace dsrrl::runtime {

struct material_response_draw_telemetry {
    std::uint64_t replacement_register_ok = 0;
    std::uint64_t replacement_register_fail = 0;
    std::uint64_t eligible_draws = 0;
    std::uint64_t replacement_miss = 0;
    std::uint64_t replay_ok = 0;
    std::uint64_t restore_fail = 0;
    bool quarantined = false;
};

class material_response_draw_runtime {
public:
    explicit material_response_draw_runtime(core::renderer_core &core) noexcept;
    ~material_response_draw_runtime();

    material_response_draw_runtime(const material_response_draw_runtime &) = delete;
    material_response_draw_runtime &operator=(const material_response_draw_runtime &) = delete;

    void on_init_device(reshade::api::device *device) noexcept;
    void on_destroy_device(reshade::api::device *device) noexcept;

    // Exact-receiver payload installation only. The caller must have already
    // attested the receiver identity and payload provenance.
    bool register_receiver_replacement(
        std::uint32_t receiver_id,
        const void *dxbc,
        std::size_t dxbc_size) noexcept;

    bool has_receiver_replacement(std::uint32_t receiver_id) const noexcept;

    bool replay_draw(
        reshade::api::command_list *cmd_list,
        std::uint32_t receiver_id,
        std::uint32_t vertex_count,
        std::uint32_t instance_count,
        std::uint32_t first_vertex,
        std::uint32_t first_instance) noexcept;

    bool replay_draw_indexed(
        reshade::api::command_list *cmd_list,
        std::uint32_t receiver_id,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    material_response_draw_telemetry telemetry() const noexcept;
    void reset() noexcept;

private:
    bool accepts_command(
        reshade::api::command_list *cmd_list,
        ID3D11PixelShader *&replacement) noexcept;

    bool begin_native_transaction(
        reshade::api::command_list *cmd_list,
        std::uint32_t receiver_id,
        ID3D11PixelShader *replacement,
        ID3D11PixelShader *&old_shader) noexcept;

    bool restore_native_transaction(
        reshade::api::command_list *cmd_list,
        ID3D11PixelShader *old_shader) noexcept;

    void release_replacements() noexcept;

    core::renderer_core &core_;
    mutable std::mutex mutex_;
    ID3D11Device *device_ = nullptr;
    std::unordered_map<std::uint32_t, ID3D11PixelShader *> replacements_;

    std::atomic<std::uint64_t> draw_serial_{0};
    std::atomic<std::uint64_t> replacement_register_ok_{0};
    std::atomic<std::uint64_t> replacement_register_fail_{0};
    std::atomic<std::uint64_t> eligible_draws_{0};
    std::atomic<std::uint64_t> replacement_miss_{0};
    std::atomic<std::uint64_t> replay_ok_{0};
    std::atomic<std::uint64_t> restore_fail_{0};
    std::atomic_bool quarantined_{false};
};

} // namespace dsrrl::runtime
