#pragma once

#include "dsrrl/operators/dof/dof_island.hpp"

#include <cstdint>

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime::dof {

struct host_dof_inputs {
    ID3D11ShaderResourceView *source_68 = nullptr;
    ID3D11ShaderResourceView *dofrate_support_t1 = nullptr;
    operators::dof::flat_mode mode =
        operators::dof::flat_mode::unknown;
    bool pass00_fragment0 = false;
    bool ready = false;
};

struct host_depth_route_telemetry {
    std::uint64_t pass01_calls = 0u;
    std::uint64_t pass0d_calls = 0u;
    std::uint64_t first_pass01_hits = 0u;
    std::uint64_t mode_primary = 0u;
    std::uint64_t mode_alternate = 0u;
    std::uint64_t source_capture_ok = 0u;
    std::uint64_t source_capture_fail = 0u;
    std::uint64_t support_capture_ok = 0u;
    std::uint64_t support_capture_fail = 0u;
    std::uint64_t abi_reject = 0u;
    std::uint32_t pass01_last_thread = 0u;
    std::uint32_t pass0d_last_thread = 0u;
    bool pass01_hook_ready = false;
    bool pass0d_hook_ready = false;
};

bool register_host_depth_route_runtime() noexcept;
void unregister_host_depth_route_runtime() noexcept;

// Valid only while the exact DSR ImageProcessDof_Flat pass 0x01 executor is
// on the current thread. A first-pass descriptor is authenticated by its
// original source resource ID:
//   mode 0: ImageState+0x78, support ImageState+0x88
//   mode 1: ImageState+0x250, support ImageState+0x230.
// source_68 is captured from the already-bound PS t0 after DSR's optional
// ResolveTAA +0x26C/+0x270 substitution. Support is resolved through the stock
// resource binder and the previous t1 resource ID is restored through that
// same binder before returning, keeping DSR's state cache coherent.
bool capture_host_dof_inputs(
    ID3D11DeviceContext *context,
    host_dof_inputs &out) noexcept;

void release_host_dof_inputs(
    host_dof_inputs &inputs) noexcept;

// DSR pass00 uses host state selector token 3 while pass01 uses token 1.
// These helpers are scoped to an authenticated first pass01 call and restore
// both host state-manager caches after the synthetic PTDE pass00.
bool push_host_pass00_state() noexcept;
bool pop_host_pass00_state() noexcept;

bool inside_exact_dof_pass01() noexcept;
bool inside_exact_dof_pass0d() noexcept;
host_depth_route_telemetry host_depth_route_status() noexcept;

} // namespace dsrrl::runtime::dof
