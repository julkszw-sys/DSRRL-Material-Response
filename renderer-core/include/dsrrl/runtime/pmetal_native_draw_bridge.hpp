#pragma once

#include "dsrrl/runtime/draw_state_transaction.hpp"

#include <reshade.hpp>

#include <atomic>
#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace dsrrl::runtime {

struct pmetal_native_draw_telemetry {
    std::uint64_t armed = 0;
    std::uint64_t draw_applied = 0;
    std::uint64_t draw_indexed_applied = 0;
    std::uint64_t arm_reject = 0;
    std::uint64_t restore_fail = 0;
    bool hook_active = false;
    bool quarantined = false;
};

class pmetal_native_draw_bridge {
public:
    pmetal_native_draw_bridge() noexcept = default;
    ~pmetal_native_draw_bridge();

    pmetal_native_draw_bridge(
        const pmetal_native_draw_bridge &) = delete;
    pmetal_native_draw_bridge &operator=(
        const pmetal_native_draw_bridge &) = delete;

    bool install(
        reshade::api::device *device) noexcept;
    void uninstall() noexcept;

    // Arms a one-shot mutation for the immediately following native Draw or
    // DrawIndexed issued by ReShade's D3D11 wrapper. Only immediate contexts
    // are accepted. The mutation is retained independently from caller state.
    bool arm_draw(
        reshade::api::command_list *cmd_list,
        const draw_tx_mutation &mutation,
        std::uint32_t vertex_count,
        std::uint32_t instance_count,
        std::uint32_t first_vertex,
        std::uint32_t first_instance) noexcept;

    bool arm_draw_indexed(
        reshade::api::command_list *cmd_list,
        const draw_tx_mutation &mutation,
        std::uint32_t index_count,
        std::uint32_t instance_count,
        std::uint32_t first_index,
        std::int32_t vertex_offset,
        std::uint32_t first_instance) noexcept;

    pmetal_native_draw_telemetry telemetry() const noexcept;
    void reset_telemetry() noexcept;

private:
    static void STDMETHODCALLTYPE draw_hook(
        ID3D11DeviceContext *context,
        unsigned int vertex_count,
        unsigned int start_vertex) noexcept;

    static void STDMETHODCALLTYPE draw_indexed_hook(
        ID3D11DeviceContext *context,
        unsigned int index_count,
        unsigned int start_index,
        int base_vertex) noexcept;

    static void STDMETHODCALLTYPE draw_instanced_hook(
        ID3D11DeviceContext *context,
        unsigned int vertex_count_per_instance,
        unsigned int instance_count,
        unsigned int start_vertex,
        unsigned int start_instance) noexcept;

    static void STDMETHODCALLTYPE draw_indexed_instanced_hook(
        ID3D11DeviceContext *context,
        unsigned int index_count_per_instance,
        unsigned int instance_count,
        unsigned int start_index,
        int base_vertex,
        unsigned int start_instance) noexcept;

    struct impl;
    impl *impl_ = nullptr;
};

} // namespace dsrrl::runtime
