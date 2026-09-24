#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <d3d11_1.h>
#include <cstdint>

namespace dsrrl::a3::ul {

// A3 owns no draw replay here. The caller must invoke prepare_draw only after all
// legacy 1.45 PRE transactions have finished and finish_draw immediately after
// the single native draw, before legacy POST begins.

struct draw_token {
    ID3D11DeviceContext *ctx = nullptr;
    ID3D11DeviceContext1 *ctx1 = nullptr;
    ID3D11PixelShader *old_ps = nullptr;
    ID3D11PixelShader *replacement_ps = nullptr;
    ID3D11Buffer *old_b13_base = nullptr;
    ID3D11Buffer *old_b13_window = nullptr;
    ID3D11Buffer *injected_b13 = nullptr;
    UINT old_first = 0;
    UINT old_num = 0;
    bool explicit_window = false;
    bool active = false;
};

// Installs only the verified DSR LightBank producer + selector freshness hooks.
// It does not register a ReShade draw callback and cannot replay a draw.
bool initialize_producer(std::uintptr_t exe_base) noexcept;
void shutdown_producer() noexcept;

// True only when selector-side owner + A/B + beta freshness resolved a current
// PTDE-linear snapshot for this thread.
bool snapshot_ready() noexcept;

// Bind the caller-selected U/L consumer pixel shader and b13[6..7] payload.
// Replacement selection is intentionally outside this module so exact
// material/receiver route gates (including P_Metal) cannot be bypassed here.
bool prepare_draw(
    ID3D11DeviceContext *ctx,
    ID3D11PixelShader *replacement_ps,
    draw_token &token) noexcept;

// Restores previous b13 (including Context1 subrange window) and previous PS,
// verifies b13 restoration, clears the selector snapshot, and quarantines the
// U/L bridge on restoration failure.
void finish_draw(draw_token &token) noexcept;

bool quarantined() noexcept;
std::uint64_t snapshots_published() noexcept;
std::uint64_t selector_matches() noexcept;
std::uint64_t prepare_passes() noexcept;
std::uint64_t prepare_failopens() noexcept;
std::uint64_t restore_failures() noexcept;

} // namespace dsrrl::a3::ul
