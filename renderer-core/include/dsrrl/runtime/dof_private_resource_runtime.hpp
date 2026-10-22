#pragma once

#include "dsrrl/operators/dof/dof_resource_contract.hpp"

#include <cstdint>

struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;

namespace dsrrl::runtime::dof {

struct private_resource_telemetry {
    std::uint64_t init_device_events = 0u;
    std::uint64_t destroy_device_events = 0u;
    std::uint64_t create_ok = 0u;
    std::uint64_t create_fail = 0u;
    std::uint64_t authorize_ok = 0u;
    std::uint64_t authorize_fail = 0u;
    std::uint64_t acquire_ok = 0u;
    std::uint64_t acquire_fail = 0u;
    bool resources_ready = false;
    bool authorized = false;
};

// Registers only resource lifetime callbacks. Resource creation is pixel inert;
// access is denied until authorize_private_resources() is called with a full
// activation context that already passes evaluate_activation().
bool register_private_resource_runtime() noexcept;
void unregister_private_resource_runtime() noexcept;

bool authorize_private_resources(
    const operators::dof::activation_context &context) noexcept;
void revoke_private_resources() noexcept;

bool acquire_private_render_target(
    operators::dof::ptde_surface_role role,
    ID3D11RenderTargetView **out) noexcept;

bool acquire_private_shader_resource(
    operators::dof::ptde_surface_role role,
    ID3D11ShaderResourceView **out) noexcept;

bool private_resources_ready() noexcept;
private_resource_telemetry private_resource_status() noexcept;

} // namespace dsrrl::runtime::dof
