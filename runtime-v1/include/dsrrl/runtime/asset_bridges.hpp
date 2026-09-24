#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <cstdint>

namespace dsrrl::core { class renderer_core; }

namespace dsrrl::runtime::assets {

struct material_route_scope {
    bool exact = false;
    bool diffuse_normal_eligible = false;
    std::uint32_t route_index = 0;
    std::array<std::uint32_t, 3> receivers{};
};

struct draw_state {
    ID3D11ShaderResourceView *old_t0 = nullptr;
    ID3D11ShaderResourceView *old_t1 = nullptr;
    ID3D11ShaderResourceView *old_t2 = nullptr;
    ID3D11ShaderResourceView *old_t10 = nullptr;
    bool changed_t0 = false;
    bool changed_t2 = false;
    bool changed_t10 = false;
};

bool register_runtime(core::renderer_core &core) noexcept;
void unregister_runtime() noexcept;

void texture_name_event(const wchar_t *logical_name) noexcept;
void texture_name_clear_event() noexcept;

bool apply_draw(
    ID3D11DeviceContext *context,
    const material_route_scope &route,
    std::uint32_t receiver_id,
    draw_state &state) noexcept;

bool restore_draw(
    ID3D11DeviceContext *context,
    draw_state &state) noexcept;

} // namespace dsrrl::runtime::assets
