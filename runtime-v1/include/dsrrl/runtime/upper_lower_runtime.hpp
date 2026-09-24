#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <array>
#include <cstdint>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::upper_lower {

struct pmetal_env_source {
    std::array<float,3> a{};
    std::array<float,3> b{};
    float beta = 0.0f;
    std::uint64_t bank_signature_a = 0;
    std::uint64_t bank_signature_b = 0;
    std::uint32_t row_id_a = 0;
    std::uint32_t row_id_b = 0;
};

struct draw_state {
    ID3D11Buffer *old_base = nullptr;
    ID3D11Buffer *old_window = nullptr;
    ID3D11Buffer *replacement = nullptr;
    UINT first = 0;
    UINT count = 0;
    bool explicit_window = false;
    bool coherent = true;
    bool bound = false;
};

bool register_runtime(core::renderer_core &core) noexcept;
void unregister_runtime() noexcept;

void selector_event(
    void *container,
    void *owner,
    void *ret,
    void *r14,
    void *r15,
    std::int32_t material_index) noexcept;

bool selected_snapshot_ready() noexcept;
bool pmetal_env_producer_ready() noexcept;
bool selected_pmetal_env_source(pmetal_env_source &out) noexcept;
bool bind_draw(ID3D11DeviceContext *context, draw_state &state) noexcept;
bool restore_draw(ID3D11DeviceContext *context, draw_state &state) noexcept;
void consume_draw_selection() noexcept;

} // namespace dsrrl::runtime::upper_lower
