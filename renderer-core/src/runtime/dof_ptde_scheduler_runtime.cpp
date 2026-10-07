#include "dsrrl/runtime/dof_ptde_scheduler_runtime.hpp"

#include "dsrrl/operators/dof/dof_resource_contract.hpp"
#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"
#include "dsrrl/runtime/dof_private_resource_runtime.hpp"
#include "dof_plain_rate_embedded.hpp"

#include <reshade.hpp>

#include <d3d11.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime::dof {
namespace {

namespace hashing = operators::legacy_plan::hashing;
using role = operators::dof::retained_shader_role;
using surface_role = operators::dof::ptde_surface_role;
using source_kind = operators::dof::production_source_kind;

struct native_pair {
    ID3D11VertexShader *vertex = nullptr;
    ID3D11PixelShader *pixel = nullptr;
};

struct saved_state {
    ID3D11VertexShader *vertex = nullptr;
    ID3D11PixelShader *pixel = nullptr;

    std::array<ID3D11ClassInstance *, 256> vs_classes{};
    std::array<ID3D11ClassInstance *, 256> ps_classes{};
    UINT vs_class_count = 0u;
    UINT ps_class_count = 0u;

    std::array<ID3D11RenderTargetView *,
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs{};
    ID3D11DepthStencilView *dsv = nullptr;

    std::array<ID3D11ShaderResourceView *, 6> srvs{};
    std::array<ID3D11SamplerState *, 6> samplers{};

    ID3D11BlendState *blend = nullptr;
    std::array<FLOAT, 4> blend_factor{};
    UINT sample_mask = 0xffffffffu;

    std::array<D3D11_VIEWPORT,
        D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
    UINT viewport_count = 0u;

    std::array<D3D11_RECT,
        D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors{};
    UINT scissor_count = 0u;

    bool captured = false;
};

std::mutex g_mutex;
ID3D11Device *g_device = nullptr;
std::array<native_pair,
    static_cast<std::size_t>(role::count)> g_pairs{};
ID3D11BlendState *g_alpha_write_blend = nullptr;
ID3D11BlendState *g_rgb_write_blend = nullptr;

std::atomic<std::uint64_t> g_init_pipeline_events{0u};
std::atomic<std::uint64_t> g_exact_shader_pairs{0u};
std::atomic<std::uint64_t> g_execute_requests{0u};
std::atomic<std::uint64_t> g_execute_ok{0u};
std::atomic<std::uint64_t> g_execute_fail{0u};
std::atomic<std::uint64_t> g_pass_draws{0u};
std::atomic<std::uint64_t> g_restore_fail{0u};
std::atomic_bool g_quarantined{false};
bool g_registered = false;

const reshade::api::shader_desc *find_shader(
    reshade::api::pipeline_subobject_type type,
    std::uint32_t count,
    const reshade::api::pipeline_subobject *subobjects) noexcept
{
    if (subobjects == nullptr)
        return nullptr;

    for (std::uint32_t i = 0u; i < count; ++i) {
        if (subobjects[i].type == type &&
            subobjects[i].count == 1u &&
            subobjects[i].data != nullptr)
            return static_cast<const reshade::api::shader_desc *>(
                subobjects[i].data);
    }

    return nullptr;
}

operators::dof::digest32 digest_of(
    const reshade::api::shader_desc &shader) noexcept
{
    operators::dof::digest32 out{};
    if (shader.code == nullptr || shader.code_size == 0u)
        return out;

    out.bytes = hashing::sha256(
        static_cast<const std::uint8_t *>(shader.code),
        shader.code_size);
    return out;
}

void release_pairs_locked() noexcept
{
    for (auto &pair : g_pairs) {
        if (pair.pixel != nullptr) {
            pair.pixel->Release();
            pair.pixel = nullptr;
        }
        if (pair.vertex != nullptr) {
            pair.vertex->Release();
            pair.vertex = nullptr;
        }
    }

    if (g_rgb_write_blend != nullptr) {
        g_rgb_write_blend->Release();
        g_rgb_write_blend = nullptr;
    }
    if (g_alpha_write_blend != nullptr) {
        g_alpha_write_blend->Release();
        g_alpha_write_blend = nullptr;
    }

    if (g_device != nullptr) {
        g_device->Release();
        g_device = nullptr;
    }
}

bool materialize_pair(
    reshade::api::device *device,
    role selected,
    const reshade::api::shader_desc &vs,
    const reshade::api::shader_desc &ps) noexcept
{
    if (device == nullptr ||
        selected == role::count ||
        vs.code == nullptr || vs.code_size == 0u ||
        ps.code == nullptr || ps.code_size == 0u)
        return false;

    auto *native = reinterpret_cast<ID3D11Device *>(
        device->get_native());
    if (native == nullptr)
        return false;

    ID3D11VertexShader *vertex = nullptr;
    if (FAILED(native->CreateVertexShader(
            vs.code,
            vs.code_size,
            nullptr,
            &vertex)) ||
        vertex == nullptr)
        return false;

    ID3D11PixelShader *pixel = nullptr;
    if (FAILED(native->CreatePixelShader(
            ps.code,
            ps.code_size,
            nullptr,
            &pixel)) ||
        pixel == nullptr) {
        vertex->Release();
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_device == nullptr) {
        g_device = native;
        g_device->AddRef();
    } else if (g_device != native) {
        vertex->Release();
        pixel->Release();
        g_quarantined.store(true);
        return false;
    }

    auto &slot = g_pairs[
        static_cast<std::size_t>(selected)];

    if (slot.vertex == nullptr &&
        slot.pixel == nullptr) {
        slot.vertex = vertex;
        slot.pixel = pixel;
        ++g_exact_shader_pairs;
        return true;
    }

    const bool coherent =
        slot.vertex != nullptr &&
        slot.pixel != nullptr;

    vertex->Release();
    pixel->Release();
    return coherent;
}

std::uint32_t retained_stage_mask(
    const reshade::api::shader_desc &shader,
    bool vertex) noexcept
{
    if (shader.code == nullptr || shader.code_size == 0u)
        return 0u;

    const auto digest = digest_of(shader);
    std::uint32_t mask = 0u;
    for (const auto &entry :
         operators::dof::retained_pipeline_signatures) {
        const auto bit =
            1u << static_cast<std::uint32_t>(entry.role);
        const bool match =
            vertex
                ? (entry.vertex_size == shader.code_size &&
                   entry.vertex_sha256 == digest)
                : (entry.pixel_size == shader.code_size &&
                   entry.pixel_sha256 == digest);
        if (match)
            mask |= bit;
    }
    return mask;
}

bool accept_device_locked(
    ID3D11Device *native) noexcept
{
    if (native == nullptr)
        return false;

    if (g_device == nullptr) {
        g_device = native;
        g_device->AddRef();
        return true;
    }

    if (g_device != native) {
        g_quarantined.store(true);
        return false;
    }
    return true;
}

bool materialize_write_masks(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return false;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr)
        return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!accept_device_locked(native))
        return false;

    if (g_alpha_write_blend != nullptr &&
        g_rgb_write_blend != nullptr)
        return true;

    D3D11_BLEND_DESC desc{};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = FALSE;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    ID3D11BlendState *alpha = nullptr;
    ID3D11BlendState *rgb = nullptr;

    desc.RenderTarget[0].RenderTargetWriteMask =
        D3D11_COLOR_WRITE_ENABLE_ALPHA;
    HRESULT hr =
        native->CreateBlendState(
            &desc,
            &alpha);

    if (SUCCEEDED(hr)) {
        desc.RenderTarget[0].RenderTargetWriteMask =
            D3D11_COLOR_WRITE_ENABLE_RED |
            D3D11_COLOR_WRITE_ENABLE_GREEN |
            D3D11_COLOR_WRITE_ENABLE_BLUE;
        hr =
            native->CreateBlendState(
                &desc,
                &rgb);
    }

    if (FAILED(hr) ||
        alpha == nullptr ||
        rgb == nullptr) {
        if (rgb != nullptr)
            rgb->Release();
        if (alpha != nullptr)
            alpha->Release();
        return false;
    }

    g_alpha_write_blend = alpha;
    g_rgb_write_blend = rgb;
    return true;
}

bool acquire_write_mask_state(
    std::uint8_t mask,
    ID3D11BlendState **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;

    if (mask == 0x0Fu)
        return true;

    std::lock_guard<std::mutex> lock(g_mutex);

    ID3D11BlendState *state = nullptr;
    if (mask == 0x08u)
        state = g_alpha_write_blend;
    else if (mask == 0x07u)
        state = g_rgb_write_blend;
    else
        return false;

    if (state == nullptr)
        return false;

    state->AddRef();
    *out = state;
    return true;
}

bool materialize_vertex_stage(
    reshade::api::device *device,
    const reshade::api::shader_desc &vs) noexcept
{
    const auto mask =
        retained_stage_mask(vs, true);
    if (device == nullptr ||
        mask == 0u ||
        vs.code == nullptr ||
        vs.code_size == 0u)
        return false;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr)
        return false;

    ID3D11VertexShader *vertex = nullptr;
    if (FAILED(native->CreateVertexShader(
            vs.code,
            vs.code_size,
            nullptr,
            &vertex)) ||
        vertex == nullptr)
        return false;

    bool admitted = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!accept_device_locked(native)) {
            vertex->Release();
            return false;
        }

        for (std::size_t i = 0u;
             i < static_cast<std::size_t>(role::count);
             ++i) {
            const auto bit = 1u << static_cast<std::uint32_t>(i);
            if ((mask & bit) == 0u)
                continue;

            auto &slot = g_pairs[i];
            const bool was_ready =
                slot.vertex != nullptr &&
                slot.pixel != nullptr;
            if (slot.vertex == nullptr) {
                vertex->AddRef();
                slot.vertex = vertex;
                admitted = true;
            }
            if (!was_ready &&
                slot.vertex != nullptr &&
                slot.pixel != nullptr)
                ++g_exact_shader_pairs;
        }
    }

    vertex->Release();
    return admitted;
}

bool materialize_pixel_stage(
    reshade::api::device *device,
    const reshade::api::shader_desc &ps) noexcept
{
    const auto mask =
        retained_stage_mask(ps, false);
    if (device == nullptr ||
        mask == 0u ||
        ps.code == nullptr ||
        ps.code_size == 0u)
        return false;

    auto *native =
        reinterpret_cast<ID3D11Device *>(
            device->get_native());
    if (native == nullptr)
        return false;

    ID3D11PixelShader *pixel = nullptr;
    if (FAILED(native->CreatePixelShader(
            ps.code,
            ps.code_size,
            nullptr,
            &pixel)) ||
        pixel == nullptr)
        return false;

    bool admitted = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!accept_device_locked(native)) {
            pixel->Release();
            return false;
        }

        for (std::size_t i = 0u;
             i < static_cast<std::size_t>(role::count);
             ++i) {
            const auto bit = 1u << static_cast<std::uint32_t>(i);
            if ((mask & bit) == 0u)
                continue;

            auto &slot = g_pairs[i];
            const bool was_ready =
                slot.vertex != nullptr &&
                slot.pixel != nullptr;
            if (slot.pixel == nullptr) {
                pixel->AddRef();
                slot.pixel = pixel;
                admitted = true;
            }
            if (!was_ready &&
                slot.vertex != nullptr &&
                slot.pixel != nullptr)
                ++g_exact_shader_pairs;
        }
    }

    pixel->Release();
    return admitted;
}

bool materialize_embedded_plain_rate(
    reshade::api::device *device) noexcept
{
    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11)
        return false;

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto &plain =
            g_pairs[static_cast<std::size_t>(
                role::dof_rate_plain)];
        if (plain.vertex != nullptr &&
            plain.pixel != nullptr)
            return true;
    }

    reshade::api::shader_desc vs{};
    vs.code =
        embedded_plain_rate::vertex.data();
    vs.code_size =
        embedded_plain_rate::vertex.size();

    reshade::api::shader_desc ps{};
    ps.code =
        embedded_plain_rate::pixel.data();
    ps.code_size =
        embedded_plain_rate::pixel.size();

    return materialize_pair(
        device,
        role::dof_rate_plain,
        vs,
        ps);
}

void on_init_device(
    reshade::api::device *device)
{
    if (g_quarantined.load())
        return;

    if (!materialize_embedded_plain_rate(
            device) ||
        !materialize_write_masks(
            device))
        g_quarantined.store(true);
}

void on_init_pipeline(
    reshade::api::device *device,
    reshade::api::pipeline_layout,
    std::uint32_t subobject_count,
    const reshade::api::pipeline_subobject *subobjects,
    reshade::api::pipeline)
{
    ++g_init_pipeline_events;

    if (device == nullptr ||
        device->get_api() !=
            reshade::api::device_api::d3d11 ||
        g_quarantined.load())
        return;

    if (!materialize_embedded_plain_rate(
            device) ||
        !materialize_write_masks(
            device)) {
        g_quarantined.store(true);
        return;
    }

    if (const auto *vs = find_shader(
            reshade::api::pipeline_subobject_type::vertex_shader,
            subobject_count,
            subobjects);
        vs != nullptr &&
        vs->code != nullptr &&
        vs->code_size != 0u)
        (void)materialize_vertex_stage(
            device,
            *vs);

    if (const auto *ps = find_shader(
            reshade::api::pipeline_subobject_type::pixel_shader,
            subobject_count,
            subobjects);
        ps != nullptr &&
        ps->code != nullptr &&
        ps->code_size != 0u)
        (void)materialize_pixel_stage(
            device,
            *ps);
}

void on_destroy_device(
    reshade::api::device *device)
{
    if (device == nullptr)
        return;

    auto *native = reinterpret_cast<ID3D11Device *>(
        device->get_native());

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_device == native)
        release_pairs_locked();
}

bool pair_ready_locked(role selected) noexcept
{
    if (selected == role::count)
        return false;

    const auto &pair =
        g_pairs[static_cast<std::size_t>(selected)];
    return pair.vertex != nullptr &&
        pair.pixel != nullptr;
}

bool execution_set_ready_locked(
    const scheduler_external_inputs &) noexcept
{
    const std::array<role, 7> required = {{
        role::dof_rate_plain,
        role::downsample,
        role::unfocus_3x3,
        role::blur_upsample,
        role::near_rate,
        role::unfocus_near_rate_3x3,
        role::dof_composite
    }};

    for (const auto selected : required)
        if (!pair_ready_locked(selected))
            return false;

    return
        g_alpha_write_blend != nullptr &&
        g_rgb_write_blend != nullptr;
}

bool acquire_pair(
    role selected,
    native_pair &out) noexcept
{
    out = {};
    std::lock_guard<std::mutex> lock(g_mutex);

    if (!pair_ready_locked(selected))
        return false;

    const auto &pair =
        g_pairs[static_cast<std::size_t>(selected)];
    pair.vertex->AddRef();
    pair.pixel->AddRef();

    out.vertex = pair.vertex;
    out.pixel = pair.pixel;
    return true;
}

void release_pair(native_pair &pair) noexcept
{
    if (pair.pixel != nullptr)
        pair.pixel->Release();
    if (pair.vertex != nullptr)
        pair.vertex->Release();
    pair = {};
}

const operators::dof::ptde_surface_pair_desc *
surface_for_role(surface_role role_value) noexcept
{
    return operators::dof::find_ptde_surface(role_value);
}

bool resolve_source(
    source_kind source,
    const scheduler_external_inputs &inputs,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;
    *out = nullptr;

    if (source == source_kind::none)
        return true;

    if (source == source_kind::scene) {
        if (!inputs.scene_source_verified ||
            inputs.scene_source_t0 == nullptr)
            return false;
        inputs.scene_source_t0->AddRef();
        *out = inputs.scene_source_t0;
        return true;
    }

    if (source == source_kind::depth) {
        if (!inputs.depth_support_verified ||
            inputs.depth_support_t1 == nullptr)
            return false;
        inputs.depth_support_t1->AddRef();
        *out = inputs.depth_support_t1;
        return true;
    }

    surface_role role_value = surface_role::count;
    switch (source) {
    case source_kind::full_prefix:
        role_value = surface_role::full_prefix;
        break;
    case source_kind::half_rate:
        role_value = surface_role::half_rate;
        break;
    case source_kind::half_rate_result:
        role_value = surface_role::half_rate_result;
        break;
    case source_kind::half_blur:
        role_value = surface_role::half_blur;
        break;
    case source_kind::quarter_ping:
        role_value = surface_role::quarter_ping;
        break;
    case source_kind::quarter_pong:
        role_value = surface_role::quarter_pong;
        break;
    default:
        return false;
    }

    return acquire_private_shader_resource(
        role_value,
        out);
}

void release_srvs(
    std::array<ID3D11ShaderResourceView *, 6> &srvs) noexcept
{
    for (auto *&srv : srvs) {
        if (srv != nullptr)
            srv->Release();
        srv = nullptr;
    }
}

bool capture_state(
    ID3D11DeviceContext *context,
    saved_state &state) noexcept
{
    state = {};
    if (context == nullptr)
        return false;

    state.vs_class_count =
        static_cast<UINT>(state.vs_classes.size());
    context->VSGetShader(
        &state.vertex,
        state.vs_classes.data(),
        &state.vs_class_count);

    state.ps_class_count =
        static_cast<UINT>(state.ps_classes.size());
    context->PSGetShader(
        &state.pixel,
        state.ps_classes.data(),
        &state.ps_class_count);

    context->OMGetRenderTargets(
        static_cast<UINT>(state.rtvs.size()),
        state.rtvs.data(),
        &state.dsv);

    context->PSGetShaderResources(
        0u,
        static_cast<UINT>(state.srvs.size()),
        state.srvs.data());

    context->PSGetSamplers(
        0u,
        static_cast<UINT>(state.samplers.size()),
        state.samplers.data());

    context->OMGetBlendState(
        &state.blend,
        state.blend_factor.data(),
        &state.sample_mask);

    state.viewport_count =
        static_cast<UINT>(state.viewports.size());
    context->RSGetViewports(
        &state.viewport_count,
        state.viewports.data());

    state.scissor_count =
        static_cast<UINT>(state.scissors.size());
    context->RSGetScissorRects(
        &state.scissor_count,
        state.scissors.data());

    state.captured =
        state.vertex != nullptr &&
        state.pixel != nullptr &&
        state.viewport_count != 0u;

    return state.captured;
}

void release_state(saved_state &state) noexcept
{
    for (UINT i = 0u; i < state.vs_class_count; ++i)
        if (state.vs_classes[i] != nullptr)
            state.vs_classes[i]->Release();

    for (UINT i = 0u; i < state.ps_class_count; ++i)
        if (state.ps_classes[i] != nullptr)
            state.ps_classes[i]->Release();

    for (auto *rtv : state.rtvs)
        if (rtv != nullptr)
            rtv->Release();

    if (state.dsv != nullptr)
        state.dsv->Release();

    for (auto *srv : state.srvs)
        if (srv != nullptr)
            srv->Release();

    for (auto *sampler : state.samplers)
        if (sampler != nullptr)
            sampler->Release();

    if (state.blend != nullptr)
        state.blend->Release();

    if (state.pixel != nullptr)
        state.pixel->Release();
    if (state.vertex != nullptr)
        state.vertex->Release();

    state = {};
}

void restore_state(
    ID3D11DeviceContext *context,
    const saved_state &state) noexcept
{
    context->VSSetShader(
        state.vertex,
        state.vs_classes.data(),
        state.vs_class_count);
    context->PSSetShader(
        state.pixel,
        state.ps_classes.data(),
        state.ps_class_count);

    context->OMSetRenderTargets(
        static_cast<UINT>(state.rtvs.size()),
        state.rtvs.data(),
        state.dsv);

    context->PSSetShaderResources(
        0u,
        static_cast<UINT>(state.srvs.size()),
        state.srvs.data());

    context->PSSetSamplers(
        0u,
        static_cast<UINT>(state.samplers.size()),
        state.samplers.data());

    context->OMSetBlendState(
        state.blend,
        state.blend_factor.data(),
        state.sample_mask);

    context->RSSetViewports(
        state.viewport_count,
        state.viewports.data());

    context->RSSetScissorRects(
        state.scissor_count,
        state.scissors.data());
}

bool verify_state(
    ID3D11DeviceContext *context,
    const saved_state &state) noexcept
{
    ID3D11VertexShader *vs = nullptr;
    ID3D11PixelShader *ps = nullptr;
    context->VSGetShader(&vs, nullptr, nullptr);
    context->PSGetShader(&ps, nullptr, nullptr);

    bool ok =
        vs == state.vertex &&
        ps == state.pixel;

    if (vs != nullptr)
        vs->Release();
    if (ps != nullptr)
        ps->Release();

    std::array<ID3D11RenderTargetView *,
        D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs{};
    ID3D11DepthStencilView *dsv = nullptr;
    context->OMGetRenderTargets(
        static_cast<UINT>(rtvs.size()),
        rtvs.data(),
        &dsv);

    for (std::size_t i = 0u; i < rtvs.size(); ++i) {
        ok = ok && rtvs[i] == state.rtvs[i];
        if (rtvs[i] != nullptr)
            rtvs[i]->Release();
    }
    ok = ok && dsv == state.dsv;
    if (dsv != nullptr)
        dsv->Release();

    std::array<ID3D11ShaderResourceView *, 6> srvs{};
    context->PSGetShaderResources(
        0u,
        static_cast<UINT>(srvs.size()),
        srvs.data());
    for (std::size_t i = 0u; i < srvs.size(); ++i) {
        ok = ok && srvs[i] == state.srvs[i];
        if (srvs[i] != nullptr)
            srvs[i]->Release();
    }

    std::array<ID3D11SamplerState *, 6> samplers{};
    context->PSGetSamplers(
        0u,
        static_cast<UINT>(samplers.size()),
        samplers.data());
    for (std::size_t i = 0u; i < samplers.size(); ++i) {
        ok = ok && samplers[i] == state.samplers[i];
        if (samplers[i] != nullptr)
            samplers[i]->Release();
    }

    ID3D11BlendState *blend = nullptr;
    std::array<FLOAT, 4> blend_factor{};
    UINT sample_mask = 0u;
    context->OMGetBlendState(
        &blend,
        blend_factor.data(),
        &sample_mask);
    ok = ok &&
        blend == state.blend &&
        sample_mask == state.sample_mask &&
        std::memcmp(
            blend_factor.data(),
            state.blend_factor.data(),
            sizeof(FLOAT) * blend_factor.size()) == 0;
    if (blend != nullptr)
        blend->Release();

    UINT viewport_count =
        static_cast<UINT>(state.viewports.size());
    std::array<D3D11_VIEWPORT,
        D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> viewports{};
    context->RSGetViewports(
        &viewport_count,
        viewports.data());

    ok = ok && viewport_count == state.viewport_count;
    if (ok && viewport_count != 0u)
        ok = std::memcmp(
            viewports.data(),
            state.viewports.data(),
            sizeof(D3D11_VIEWPORT) * viewport_count) == 0;

    UINT scissor_count =
        static_cast<UINT>(state.scissors.size());
    std::array<D3D11_RECT,
        D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE> scissors{};
    context->RSGetScissorRects(
        &scissor_count,
        scissors.data());

    ok = ok && scissor_count == state.scissor_count;
    if (ok && scissor_count != 0u)
        ok = std::memcmp(
            scissors.data(),
            state.scissors.data(),
            sizeof(D3D11_RECT) * scissor_count) == 0;

    return ok;
}

bool issue_draw(
    ID3D11DeviceContext *context,
    const scheduler_draw_shape &shape) noexcept
{
    if (context == nullptr ||
        shape.instance_count == 0u)
        return false;

    if (shape.indexed) {
        if (shape.index_count == 0u)
            return false;

        if (shape.instance_count == 1u &&
            shape.first_instance == 0u) {
            context->DrawIndexed(
                shape.index_count,
                shape.first_index,
                shape.vertex_offset);
        } else {
            context->DrawIndexedInstanced(
                shape.index_count,
                shape.instance_count,
                shape.first_index,
                shape.vertex_offset,
                shape.first_instance);
        }
        return true;
    }

    if (shape.vertex_count == 0u)
        return false;

    if (shape.instance_count == 1u &&
        shape.first_instance == 0u) {
        context->Draw(
            shape.vertex_count,
            shape.first_vertex);
    } else {
        context->DrawInstanced(
            shape.vertex_count,
            shape.instance_count,
            shape.first_vertex,
            shape.first_instance);
    }

    return true;
}

bool run_pass(
    ID3D11DeviceContext *context,
    const operators::dof::production_seed_pass_contract &pass,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape) noexcept
{
    if (context == nullptr ||
        !inputs.pass_state_verified ||
        inputs.color_sampler == nullptr ||
        inputs.depth_sampler == nullptr)
        return false;

    const auto *target =
        surface_for_role(pass.target);
    if (target == nullptr)
        return false;

    ID3D11RenderTargetView *rtv = nullptr;
    if (!acquire_private_render_target(
            target->role,
            &rtv) ||
        rtv == nullptr)
        return false;

    native_pair shader{};
    if (!acquire_pair(
            pass.shader,
            shader)) {
        rtv->Release();
        return false;
    }

    std::array<ID3D11ShaderResourceView *, 6> srvs{};
    bool sources_ok = true;
    for (std::size_t i = 0u; i < pass.sources.size(); ++i) {
        if (!resolve_source(
                pass.sources[i],
                inputs,
                &srvs[i])) {
            sources_ok = false;
            break;
        }
    }

    if (!sources_ok) {
        release_srvs(srvs);
        release_pair(shader);
        rtv->Release();
        return false;
    }

    const std::array<ID3D11ShaderResourceView *, 6> null_srvs{};
    context->PSSetShaderResources(
        0u,
        static_cast<UINT>(null_srvs.size()),
        null_srvs.data());

    context->OMSetRenderTargets(
        1u,
        &rtv,
        nullptr);

    if (!pass.inherit_host_om) {
        ID3D11BlendState *write_state = nullptr;
        if (!acquire_write_mask_state(
                pass.rt0_write_mask,
                &write_state)) {
            release_srvs(srvs);
            release_pair(shader);
            rtv->Release();
            return false;
        }

        context->OMSetBlendState(
            write_state,
            nullptr,
            0xffffffffu);

        if (write_state != nullptr)
            write_state->Release();
    }

    D3D11_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width =
        static_cast<float>(target->raster.width);
    viewport.Height =
        static_cast<float>(target->raster.height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1u, &viewport);

    D3D11_RECT scissor{};
    scissor.left = 0;
    scissor.top = 0;
    scissor.right =
        static_cast<LONG>(target->raster.width);
    scissor.bottom =
        static_cast<LONG>(target->raster.height);
    context->RSSetScissorRects(1u, &scissor);

    std::array<ID3D11SamplerState *, 6> samplers{};
    for (std::size_t i = 0u; i < pass.sources.size(); ++i) {
        if (pass.sources[i] == source_kind::none)
            continue;
        samplers[i] =
            pass.sources[i] == source_kind::depth
                ? inputs.depth_sampler
                : inputs.color_sampler;
    }

    context->VSSetShader(
        shader.vertex,
        nullptr,
        0u);
    context->PSSetShader(
        shader.pixel,
        nullptr,
        0u);
    context->PSSetSamplers(
        0u,
        static_cast<UINT>(samplers.size()),
        samplers.data());
    context->PSSetShaderResources(
        0u,
        static_cast<UINT>(srvs.size()),
        srvs.data());

    const bool drawn =
        issue_draw(
            context,
            shape);

    if (drawn)
        ++g_pass_draws;

    context->PSSetShaderResources(
        0u,
        static_cast<UINT>(null_srvs.size()),
        null_srvs.data());

    release_srvs(srvs);
    release_pair(shader);
    rtv->Release();
    return drawn;
}

} // namespace

bool register_ptde_scheduler_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_registered)
        return true;

    try {
        g_quarantined.store(false);

        reshade::register_event<
            reshade::addon_event::init_device>(
                on_init_device);
        reshade::register_event<
            reshade::addon_event::init_pipeline>(
                on_init_pipeline);
        reshade::register_event<
            reshade::addon_event::destroy_device>(
                on_destroy_device);
        g_registered = true;
        return true;
    } catch (...) {
        return false;
    }
}

void unregister_ptde_scheduler_runtime() noexcept
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_registered) {
            release_pairs_locked();
            return;
        }
        g_registered = false;
    }

    reshade::unregister_event<
        reshade::addon_event::destroy_device>(
            on_destroy_device);
    reshade::unregister_event<
        reshade::addon_event::init_pipeline>(
            on_init_pipeline);
    reshade::unregister_event<
        reshade::addon_event::init_device>(
            on_init_device);

    std::lock_guard<std::mutex> lock(g_mutex);
    release_pairs_locked();
}

bool ptde_scheduler_plain_rate_ready() noexcept
{
    if (g_quarantined.load())
        return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    return pair_ready_locked(
        role::dof_rate_plain);
}

bool ptde_scheduler_execution_set_ready(
    const scheduler_external_inputs &inputs) noexcept
{
    if (g_quarantined.load() ||
        !private_resources_ready())
        return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    return execution_set_ready_locked(inputs);
}

scheduler_result execute_ptde_pass(
    reshade::api::command_list *cmd_list,
    const operators::dof::activation_context &activation,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape,
    std::size_t pass_index) noexcept
{
    ++g_execute_requests;

    if (g_quarantined.load() ||
        cmd_list == nullptr ||
        pass_index >= operators::dof::production_seed_passes.size() ||
        !operators::dof::evaluate_activation(activation).active) {
        ++g_execute_fail;
        return scheduler_result::not_ready;
    }

    const auto &pass =
        operators::dof::production_seed_passes[pass_index];
    const role selected = pass.shader;

    if (selected == role::count ||
        !inputs.scene_source_verified ||
        inputs.scene_source_t0 == nullptr ||
        !inputs.depth_support_verified ||
        inputs.depth_support_t1 == nullptr ||
        !inputs.pass_state_verified) {
        ++g_execute_fail;
        return scheduler_result::input_rejected;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!pair_ready_locked(selected)) {
            ++g_execute_fail;
            return scheduler_result::not_ready;
        }
    }

    if (!private_resources_ready() ||
        !authorize_private_resources(activation)) {
        ++g_execute_fail;
        return scheduler_result::not_ready;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr) {
        revoke_private_resources();
        ++g_execute_fail;
        return scheduler_result::input_rejected;
    }

    saved_state state{};
    if (!capture_state(context, state)) {
        revoke_private_resources();
        release_state(state);
        ++g_execute_fail;
        return scheduler_result::state_capture_failed;
    }

    const bool pass_ok =
        run_pass(
            context,
            pass,
            inputs,
            shape);

    restore_state(context, state);
    const bool restored =
        verify_state(context, state);
    release_state(state);
    revoke_private_resources();

    if (!restored) {
        ++g_restore_fail;
        ++g_execute_fail;
        g_quarantined.store(true);
        return scheduler_result::restore_failed;
    }

    if (!pass_ok) {
        ++g_execute_fail;
        return scheduler_result::pass_failed;
    }

    ++g_execute_ok;
    return scheduler_result::executed;
}

scheduler_result execute_ptde_graph(
    reshade::api::command_list *cmd_list,
    const operators::dof::activation_context &activation,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape,
    ID3D11ShaderResourceView **terminal_output) noexcept
{
    ++g_execute_requests;
    (void)shape;

    if (terminal_output == nullptr) {
        ++g_execute_fail;
        return scheduler_result::input_rejected;
    }
    *terminal_output = nullptr;

    if (g_quarantined.load() ||
        cmd_list == nullptr ||
        !operators::dof::evaluate_activation(activation).active ||
        activation.carrier !=
            operators::dof::carrier_mode::native_rate_ptde_seed ||
        !inputs.scene_source_verified ||
        inputs.scene_source_t0 == nullptr ||
        !inputs.depth_support_verified ||
        inputs.depth_support_t1 == nullptr ||
        !inputs.pass_state_verified ||
        !ptde_scheduler_execution_set_ready(inputs)) {
        ++g_execute_fail;
        return scheduler_result::not_ready;
    }

    if (!authorize_private_resources(activation)) {
        ++g_execute_fail;
        return scheduler_result::not_ready;
    }

    auto *context =
        reinterpret_cast<ID3D11DeviceContext *>(
            cmd_list->get_native());
    if (context == nullptr) {
        revoke_private_resources();
        ++g_execute_fail;
        return scheduler_result::input_rejected;
    }

    saved_state state{};
    if (!capture_state(context, state)) {
        revoke_private_resources();
        release_state(state);
        ++g_execute_fail;
        return scheduler_result::state_capture_failed;
    }

    // The production half-seed terminal must execute inside the authentic
    // retained DSR FRPG_Fil_Dof draw. A single-shot call cannot prove that
    // terminal scope, so this API deliberately fails open.
    bool pass_ok = false;

    if (pass_ok) {
        pass_ok =
            acquire_private_shader_resource(
                surface_role::full_terminal,
                terminal_output) &&
            *terminal_output != nullptr;
    }

    restore_state(context, state);
    const bool restored =
        verify_state(context, state);
    release_state(state);
    revoke_private_resources();

    if (!restored) {
        if (*terminal_output != nullptr) {
            (*terminal_output)->Release();
            *terminal_output = nullptr;
        }
        ++g_restore_fail;
        ++g_execute_fail;
        g_quarantined.store(true);
        return scheduler_result::restore_failed;
    }

    if (!pass_ok) {
        if (*terminal_output != nullptr) {
            (*terminal_output)->Release();
            *terminal_output = nullptr;
        }
        ++g_execute_fail;
        return scheduler_result::pass_failed;
    }

    ++g_execute_ok;
    return scheduler_result::executed;
}

scheduler_telemetry ptde_scheduler_status() noexcept
{
    bool ready = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ready =
            pair_ready_locked(role::dof_rate_plain) &&
            pair_ready_locked(role::downsample) &&
            pair_ready_locked(role::unfocus_3x3) &&
            pair_ready_locked(role::blur_upsample) &&
            pair_ready_locked(role::near_rate) &&
            pair_ready_locked(role::unfocus_near_rate_3x3) &&
            pair_ready_locked(role::dof_composite);
    }

    return {
        g_init_pipeline_events.load(),
        g_exact_shader_pairs.load(),
        g_execute_requests.load(),
        g_execute_ok.load(),
        g_execute_fail.load(),
        g_pass_draws.load(),
        g_restore_fail.load(),
        ready && private_resources_ready(),
        g_quarantined.load()
    };
}

} // namespace dsrrl::runtime::dof
