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

    std::array<ID3D11ShaderResourceView *, 5> srvs{};

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

role role_for_pass(std::uint8_t pass) noexcept
{
    switch (pass) {
    case 0x00u:
        return role::dof_rate_plain;
    case 0x01u:
    case 0x02u:
        return role::downsample;
    case 0x03u:
        return role::near_rate;
    case 0x0Du:
        return role::unfocus_3x3;
    case 0x0Eu:
        return role::unfocus_near_rate_3x3;
    case 0x0Fu:
        return role::blur_upsample;
    case 0x10u:
        return role::dof_composite;
    default:
        return role::count;
    }
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

    return true;
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
surface_for_target(std::uint16_t offset) noexcept
{
    for (const auto &entry :
         operators::dof::ptde_fixed_surface_pairs)
        if (entry.target_offset == offset)
            return &entry;
    return nullptr;
}

const operators::dof::ptde_surface_pair_desc *
surface_for_srv(std::uint16_t offset) noexcept
{
    for (const auto &entry :
         operators::dof::ptde_fixed_surface_pairs)
        if (entry.srv_offset == offset)
            return &entry;
    return nullptr;
}

bool resolve_srv(
    std::uint16_t offset,
    const scheduler_external_inputs &inputs,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;

    *out = nullptr;

    if (offset == 0u)
        return true;

    if (offset == 0x0068u) {
        if (!inputs.depth_support_verified ||
            inputs.depth_support_t1 == nullptr)
            return false;
        inputs.depth_support_t1->AddRef();
        *out = inputs.depth_support_t1;
        return true;
    }

    const auto *surface =
        surface_for_srv(offset);
    if (surface == nullptr)
        return false;

    return acquire_private_shader_resource(
        surface->role,
        out);
}

void release_srvs(
    std::array<ID3D11ShaderResourceView *, 5> &srvs) noexcept
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

    std::array<ID3D11ShaderResourceView *, 5> srvs{};
    context->PSGetShaderResources(
        0u,
        static_cast<UINT>(srvs.size()),
        srvs.data());
    for (std::size_t i = 0u; i < srvs.size(); ++i) {
        ok = ok && srvs[i] == state.srvs[i];
        if (srvs[i] != nullptr)
            srvs[i]->Release();
    }

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
    const operators::dof::ptde_pass_resource_contract &pass,
    const scheduler_external_inputs &inputs,
    const scheduler_draw_shape &shape) noexcept
{
    if (context == nullptr)
        return false;

    const auto *target =
        surface_for_target(pass.target_offset);
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
            role_for_pass(pass.pass),
            shader)) {
        rtv->Release();
        return false;
    }

    std::array<ID3D11ShaderResourceView *, 5> srvs{};
    const std::array<std::uint16_t, 5> source_offsets = {{
        pass.arg6_source,
        pass.arg7_source,
        pass.arg8_source,
        pass.arg9_source,
        pass.arg10_source
    }};

    bool sources_ok = true;
    for (std::size_t i = 0u; i < source_offsets.size(); ++i) {
        if (!resolve_srv(
                source_offsets[i],
                inputs,
                &srvs[i])) {
            sources_ok = false;
            break;
        }
    }

    // PTDE pass00 is FRPG_Fil_Dof_DofRate. Its ctor carries +0x68 as
    // raw arg6, but the handler binds that resource to sampler stage1 and
    // the shader consumes only s1. Do not positional-map raw arg6 to t0.
    if (sources_ok && pass.pass == 0x00u) {
        if (srvs[1] != nullptr) {
            srvs[1]->Release();
            srvs[1] = nullptr;
        }
        srvs[1] = srvs[0];
        srvs[0] = nullptr;
    }

    if (!sources_ok) {
        release_srvs(srvs);
        release_pair(shader);
        rtv->Release();
        return false;
    }

    const std::array<ID3D11ShaderResourceView *, 5> null_srvs{};
    context->PSSetShaderResources(
        0u,
        static_cast<UINT>(null_srvs.size()),
        null_srvs.data());

    context->OMSetRenderTargets(
        1u,
        &rtv,
        nullptr);

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

    context->VSSetShader(
        shader.vertex,
        nullptr,
        0u);
    context->PSSetShader(
        shader.pixel,
        nullptr,
        0u);

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
        pass_index >= operators::dof::ptde_exact_pass_resources.size() ||
        !operators::dof::evaluate_activation(activation).active) {
        ++g_execute_fail;
        return scheduler_result::not_ready;
    }

    const auto &pass =
        operators::dof::ptde_exact_pass_resources[pass_index];
    const role selected =
        role_for_pass(pass.pass);

    if (selected == role::count ||
        ((pass.pass == 0x00u ||
          pass.pass == 0x03u ||
          pass.pass == 0x10u) &&
         (!inputs.depth_support_verified ||
          inputs.depth_support_t1 == nullptr))) {
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

    if (terminal_output == nullptr) {
        ++g_execute_fail;
        return scheduler_result::input_rejected;
    }
    *terminal_output = nullptr;

    if (g_quarantined.load() ||
        cmd_list == nullptr ||
        !operators::dof::evaluate_activation(activation).active ||
        !inputs.scene_history_q8_verified ||
        inputs.scene_history_q8 == nullptr ||
        !inputs.depth_support_verified ||
        inputs.depth_support_t1 == nullptr ||
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

    bool pass_ok = true;
    for (const auto &pass :
         operators::dof::ptde_exact_pass_resources) {
        if (!run_pass(
                context,
                pass,
                inputs,
                shape)) {
            pass_ok = false;
            break;
        }
    }

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
