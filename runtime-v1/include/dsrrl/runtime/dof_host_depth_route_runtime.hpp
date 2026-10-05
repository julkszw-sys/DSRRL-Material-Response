#pragma once

#include <cstdint>

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime::dof {

struct host_depth_route_telemetry {
    std::uint64_t pass0d_calls = 0u;
    std::uint64_t tls_hits = 0u;
    std::uint64_t selector_88 = 0u;
    std::uint64_t selector_c0 = 0u;
    std::uint64_t bind_ok = 0u;
    std::uint64_t bind_fail = 0u;
    std::uint64_t abi_reject = 0u;
    bool hook_ready = false;
};

bool register_host_depth_route_runtime() noexcept;
void unregister_host_depth_route_runtime() noexcept;

// Valid only while the exact DSR ImageProcessDof_Flat pass 0x0D executor is
// active on this thread. Replays the stock pass-0 depth routing rule:
//   render_context+0x24B8 == 0 ? ImageState+0x88 : ImageState+0xC0
// through the stock PS-SRV binder and returns the resulting t1 SRV.
bool acquire_host_depth_support_t1(
    ID3D11DeviceContext *context,
    ID3D11ShaderResourceView **out) noexcept;

bool inside_exact_dof_pass01() noexcept;
bool inside_exact_dof_pass0d() noexcept;
host_depth_route_telemetry host_depth_route_status() noexcept;

} // namespace dsrrl::runtime::dof
