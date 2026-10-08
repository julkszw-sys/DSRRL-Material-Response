#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_private_resource_runtime.hpp"

#include <reshade.hpp>

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace dsrrl::runtime::dof {
namespace {

using operators::dof::ptde_fixed_surface_pairs;
using operators::dof::ptde_surface_role;

struct surface_storage {
    ID3D11Texture2D *texture = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    ID3D11ShaderResourceView *srv = nullptr;
};

std::mutex g_mutex;
ID3D11Device *g_device = nullptr;
std::array<surface_storage, ptde_fixed_surface_pairs.size()> g_surfaces{};
private_resource_telemetry g_telemetry{};
bool g_registered = false;

constexpr std::size_t role_index(ptde_surface_role role) noexcept
{
    return static_cast<std::size_t>(role);
}

void release_surface(surface_storage &surface) noexcept
{
    if (surface.srv != nullptr) {
        surface.srv->Release();
        surface.srv = nullptr;
    }

    if (surface.rtv != nullptr) {
        surface.rtv->Release();
        surface.rtv = nullptr;
    }

    if (surface.texture != nullptr) {
        surface.texture->Release();
        surface.texture = nullptr;
    }
}

void release_locked() noexcept
{
    for (auto &surface : g_surfaces)
        release_surface(surface);

    if (g_device != nullptr) {
        g_device->Release();
        g_device = nullptr;
    }

    g_telemetry.resources_ready = false;
    g_telemetry.authorized = false;
}

bool create_surface(
    ID3D11Device *device,
    std::size_t index) noexcept
{
    if (device == nullptr ||
        index >= ptde_fixed_surface_pairs.size())
        return false;

    const auto &contract = ptde_fixed_surface_pairs[index];

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = contract.raster.width;
    desc.Height = contract.raster.height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.SampleDesc.Quality = 0u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0u;
    desc.MiscFlags = 0u;

    auto &surface = g_surfaces[index];

    HRESULT hr =
        device->CreateTexture2D(
            &desc,
            nullptr,
            &surface.texture);

    if (SUCCEEDED(hr))
        hr =
            device->CreateRenderTargetView(
                surface.texture,
                nullptr,
                &surface.rtv);

    if (SUCCEEDED(hr))
        hr =
            device->CreateShaderResourceView(
                surface.texture,
                nullptr,
                &surface.srv);

    if (FAILED(hr) ||
        surface.texture == nullptr ||
        surface.rtv == nullptr ||
        surface.srv == nullptr) {
        release_surface(surface);
        return false;
    }

    return true;
}

void on_init_device(reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_telemetry.init_device_events;

    release_locked();

    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11) {
        ++g_telemetry.create_fail;
        return;
    }

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr) {
        ++g_telemetry.create_fail;
        return;
    }

    native->AddRef();
    g_device = native;

    for (std::size_t i = 0u;
         i < g_surfaces.size();
         ++i) {
        if (!create_surface(native, i)) {
            ++g_telemetry.create_fail;
            release_locked();
            return;
        }
    }

    g_telemetry.resources_ready = true;
    g_telemetry.authorized = false;
    ++g_telemetry.create_ok;
}

void on_destroy_device(reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_telemetry.destroy_device_events;

    if (g_device == nullptr) {
        release_locked();
        return;
    }

    auto *native =
        device != nullptr
            ? reinterpret_cast<ID3D11Device *>(
                device->get_native())
            : nullptr;

    if (native == nullptr || native == g_device)
        release_locked();
}

} // namespace

bool register_private_resource_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_registered)
        return true;

    reshade::register_event<
        reshade::addon_event::init_device>(
            on_init_device);
    reshade::register_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);

    g_registered = true;
    return true;
}

void unregister_private_resource_runtime() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_registered) {
            release_locked();
            return;
        }
        g_registered = false;
    }

    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);
    reshade::unregister_event<
        reshade::addon_event::init_device>(
            on_init_device);

    std::lock_guard<std::mutex> lock(g_mutex);
    release_locked();
}

bool authorize_private_resources(
    const operators::dof::activation_context &context) noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!g_telemetry.resources_ready ||
        !operators::dof::evaluate_activation(context).active) {
        g_telemetry.authorized = false;
        ++g_telemetry.authorize_fail;
        return false;
    }

    g_telemetry.authorized = true;
    ++g_telemetry.authorize_ok;
    return true;
}

void revoke_private_resources() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_telemetry.authorized = false;
}

bool acquire_private_render_target(
    ptde_surface_role role,
    ID3D11RenderTargetView **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);

    const auto index = role_index(role);
    if (!g_telemetry.resources_ready ||
        !g_telemetry.authorized ||
        index >= g_surfaces.size() ||
        g_surfaces[index].rtv == nullptr) {
        ++g_telemetry.acquire_fail;
        return false;
    }

    g_surfaces[index].rtv->AddRef();
    *out = g_surfaces[index].rtv;
    ++g_telemetry.acquire_ok;
    return true;
}

bool acquire_private_shader_resource(
    ptde_surface_role role,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);

    const auto index = role_index(role);
    if (!g_telemetry.resources_ready ||
        !g_telemetry.authorized ||
        index >= g_surfaces.size() ||
        g_surfaces[index].srv == nullptr) {
        ++g_telemetry.acquire_fail;
        return false;
    }

    g_surfaces[index].srv->AddRef();
    *out = g_surfaces[index].srv;
    ++g_telemetry.acquire_ok;
    return true;
}

bool private_resources_ready() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_telemetry.resources_ready;
}

private_resource_telemetry private_resource_status() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_telemetry;
}

} // namespace dsrrl::runtime::dof
