#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/bloom_scene_sidecar_runtime.hpp"

#include <Windows.h>
#include <d3d11.h>

namespace dsrrl::runtime {
namespace {

using namespace operators::postprocess;

void release_interface(IUnknown *&p) noexcept
{
    if (p != nullptr) {
        p->Release();
        p = nullptr;
    }
}

} // namespace

bloom_scene_sidecar_runtime::~bloom_scene_sidecar_runtime()
{
    reset();
}

void bloom_scene_sidecar_runtime::release_locked() noexcept
{
    if (srv_ != nullptr) {
        srv_->Release();
        srv_ = nullptr;
    }

    if (rtv_ != nullptr) {
        rtv_->Release();
        rtv_ = nullptr;
    }

    if (texture_ != nullptr) {
        texture_->Release();
        texture_ = nullptr;
    }

    if (device_ != nullptr) {
        device_->Release();
        device_ = nullptr;
    }

    telemetry_.resource_ready = false;
    telemetry_.proof_authorized = false;
    telemetry_.contents_valid = false;
    telemetry_.frame_serial = 0u;
}

void bloom_scene_sidecar_runtime::on_init_device(
    reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++telemetry_.init_calls;

    if (device == nullptr ||
        device->get_api() != reshade::api::device_api::d3d11) {
        ++telemetry_.create_fail;
        return;
    }

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr) {
        ++telemetry_.create_fail;
        return;
    }

    release_locked();

    native->AddRef();
    device_ = native;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = k_ptde_q8_scene_width;
    desc.Height = k_ptde_q8_scene_height;
    desc.MipLevels = 1u;
    desc.ArraySize = 1u;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1u;
    desc.SampleDesc.Quality = 0u;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags =
        D3D11_BIND_RENDER_TARGET |
        D3D11_BIND_SHADER_RESOURCE;

    ID3D11Texture2D *texture = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    ID3D11ShaderResourceView *srv = nullptr;

    HRESULT hr =
        native->CreateTexture2D(
            &desc,
            nullptr,
            &texture);

    if (SUCCEEDED(hr))
        hr =
            native->CreateRenderTargetView(
                texture,
                nullptr,
                &rtv);

    if (SUCCEEDED(hr))
        hr =
            native->CreateShaderResourceView(
                texture,
                nullptr,
                &srv);

    if (FAILED(hr) ||
        texture == nullptr ||
        rtv == nullptr ||
        srv == nullptr) {
        if (srv != nullptr)
            srv->Release();
        if (rtv != nullptr)
            rtv->Release();
        if (texture != nullptr)
            texture->Release();

        ++telemetry_.create_fail;
        release_locked();
        return;
    }

    texture_ = texture;
    rtv_ = rtv;
    srv_ = srv;

    telemetry_.resource_ready = true;
    telemetry_.proof_authorized = false;
    telemetry_.contents_valid = false;
    telemetry_.frame_serial = 0u;
    ++telemetry_.create_ok;
}

void bloom_scene_sidecar_runtime::on_destroy_device(
    reshade::api::device *device) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++telemetry_.destroy_calls;

    if (device_ == nullptr) {
        release_locked();
        return;
    }

    auto *native =
        device != nullptr
            ? reinterpret_cast<ID3D11Device *>(
                device->get_native())
            : nullptr;

    if (native == nullptr ||
        native == device_)
        release_locked();
}

bool bloom_scene_sidecar_runtime::authorize(
    const bloom_scene_bridge_carrier &carrier) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    const bool exact =
        validate_bloom_scene_bridge_carrier(
            carrier) ==
            bloom_scene_bridge_result::exact_construction &&
        carrier.strategy ==
            bloom_scene_construction_strategy::
                history_preserving_sidecar;

    if (!telemetry_.resource_ready ||
        !exact) {
        telemetry_.proof_authorized = false;
        telemetry_.contents_valid = false;
        ++telemetry_.authorize_fail;
        return false;
    }

    telemetry_.proof_authorized = true;
    telemetry_.contents_valid = false;
    telemetry_.frame_serial = 0u;
    ++telemetry_.authorize_ok;
    return true;
}

void bloom_scene_sidecar_runtime::revoke() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    telemetry_.proof_authorized = false;
    telemetry_.contents_valid = false;
    telemetry_.frame_serial = 0u;
}

bool bloom_scene_sidecar_runtime::begin_frame(
    std::uint64_t frame_serial) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!telemetry_.resource_ready ||
        !telemetry_.proof_authorized ||
        frame_serial == 0u) {
        ++telemetry_.begin_fail;
        return false;
    }

    telemetry_.frame_serial = frame_serial;
    telemetry_.contents_valid = false;
    ++telemetry_.begin_ok;
    return true;
}

bool bloom_scene_sidecar_runtime::commit_frame(
    std::uint64_t frame_serial) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!telemetry_.resource_ready ||
        !telemetry_.proof_authorized ||
        frame_serial == 0u ||
        telemetry_.frame_serial != frame_serial) {
        ++telemetry_.commit_fail;
        return false;
    }

    telemetry_.contents_valid = true;
    ++telemetry_.commit_ok;
    return true;
}

bool bloom_scene_sidecar_runtime::acquire_render_target(
    std::uint64_t frame_serial,
    ID3D11RenderTargetView **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;
    std::lock_guard<std::mutex> lock(mutex_);

    if (!telemetry_.resource_ready ||
        !telemetry_.proof_authorized ||
        frame_serial == 0u ||
        telemetry_.frame_serial != frame_serial ||
        rtv_ == nullptr) {
        ++telemetry_.acquire_fail;
        return false;
    }

    rtv_->AddRef();
    *out = rtv_;
    ++telemetry_.rtv_acquire_ok;
    return true;
}

bool bloom_scene_sidecar_runtime::acquire_shader_resource(
    std::uint64_t frame_serial,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;
    std::lock_guard<std::mutex> lock(mutex_);

    if (!telemetry_.resource_ready ||
        !telemetry_.proof_authorized ||
        !telemetry_.contents_valid ||
        frame_serial == 0u ||
        telemetry_.frame_serial != frame_serial ||
        srv_ == nullptr) {
        ++telemetry_.acquire_fail;
        return false;
    }

    srv_->AddRef();
    *out = srv_;
    ++telemetry_.srv_acquire_ok;
    return true;
}

bloom_scene_sidecar_telemetry
bloom_scene_sidecar_runtime::telemetry() const noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    return telemetry_;
}

void bloom_scene_sidecar_runtime::reset() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    release_locked();
    telemetry_ = {};
}

} // namespace dsrrl::runtime
