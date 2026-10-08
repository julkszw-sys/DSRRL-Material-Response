#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_host_depth_route_runtime.hpp"

#include "dsrrl/runtime/dof_process_memory.hpp"

#include <Windows.h>
#include <d3d11.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace dsrrl::runtime::dof {
namespace {

constexpr std::uintptr_t k_rva_pass01 = 0x00455960u;
constexpr std::uintptr_t k_rva_pass0d = 0x00456460u;
constexpr std::uintptr_t k_rva_srv_bind = 0x000BADC0u;
constexpr std::uintptr_t k_rva_filter_state = 0x000BACD0u;
constexpr std::uintptr_t k_rva_aux_state = 0x00064FD0u;
constexpr std::uintptr_t k_rva_pass00_variant = 0x0018B930u;
constexpr std::uintptr_t k_rva_image_state_global = 0x01C6D598u;
constexpr std::size_t k_stolen = 16u;

constexpr std::array<std::uint8_t, k_stolen> k_expected = {{
    0x48,0x89,0x5C,0x24,0x20,
    0x55,
    0x56,
    0x57,
    0x41,0x56,
    0x41,0x57,
    0x48,0x83,0xEC,0x40
}};

using pass_fn = void(__fastcall *)(void *, void *, void *);
using srv_bind_fn = void(__fastcall *)(void *, std::uint32_t, std::uint32_t);
using filter_state_fn = void(__fastcall *)(void *, std::uint32_t);
using aux_state_fn = void(__fastcall *)(void *, std::uint32_t, std::uint32_t);
using pass00_variant_fn = bool(__fastcall *)(void *);

struct hook_site {
    std::uintptr_t rva = 0u;
    void *target = nullptr;
    void *trampoline = nullptr;
    pass_fn original = nullptr;
    std::array<std::uint8_t, k_stolen> original_bytes{};
    bool patched = false;
};

struct pass00_restore_state {
    void *render_context = nullptr;
    void *aux_object = nullptr;
    std::uint32_t filter_token = 0u;
    std::uint32_t aux_edx = 0u;
    std::uint32_t aux_r8 = 0u;
    bool active = false;
};

std::mutex g_mutex;
std::uintptr_t g_base = 0u;
hook_site g_pass01_site{k_rva_pass01};
hook_site g_pass0d_site{k_rva_pass0d};

thread_local void *g_pass01_node = nullptr;
thread_local void *g_pass01_render_context = nullptr;
thread_local void *g_pass01_desc = nullptr;
thread_local unsigned g_pass01_depth = 0u;

thread_local void *g_pass0d_render_context = nullptr;
thread_local unsigned g_pass0d_depth = 0u;

thread_local pass00_restore_state g_pass00_restore{};

std::atomic<std::uint64_t> g_pass01_calls{0u};
std::atomic<std::uint64_t> g_pass0d_calls{0u};
std::atomic<std::uint64_t> g_first_pass01_hits{0u};
std::atomic<std::uint64_t> g_mode_primary{0u};
std::atomic<std::uint64_t> g_mode_alternate{0u};
std::atomic<std::uint64_t> g_source_capture_ok{0u};
std::atomic<std::uint64_t> g_source_capture_fail{0u};
std::atomic<std::uint64_t> g_support_capture_ok{0u};
std::atomic<std::uint64_t> g_support_capture_fail{0u};
std::atomic<std::uint64_t> g_abi_reject{0u};
std::atomic<std::uint32_t> g_pass01_last_thread{0u};
std::atomic<std::uint32_t> g_pass0d_last_thread{0u};

bool write_bytes(
    void *address,
    const void *bytes,
    std::size_t size) noexcept
{
    DWORD old = 0u;
    if (address == nullptr ||
        bytes == nullptr ||
        size == 0u ||
        !VirtualProtect(
            address,
            size,
            PAGE_EXECUTE_READWRITE,
            &old))
        return false;

    std::memcpy(address, bytes, size);
    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            address,
            size) != FALSE;

    DWORD ignored = 0u;
    const bool restored =
        VirtualProtect(
            address,
            size,
            old,
            &ignored) != FALSE;

    return flushed && restored;
}

void __fastcall hook_pass01(
    void *node,
    void *render_context,
    void *pass_desc) noexcept
{
    ++g_pass01_calls;
    g_pass01_last_thread.store(
        static_cast<std::uint32_t>(GetCurrentThreadId()),
        std::memory_order_relaxed);

    void *const previous_node = g_pass01_node;
    void *const previous_context = g_pass01_render_context;
    void *const previous_desc = g_pass01_desc;
    const unsigned previous_depth = g_pass01_depth;

    g_pass01_node = node;
    g_pass01_render_context = render_context;
    g_pass01_desc = pass_desc;
    g_pass01_depth = previous_depth + 1u;

    if (g_pass01_site.original != nullptr)
        g_pass01_site.original(node, render_context, pass_desc);

    g_pass01_depth = previous_depth;
    g_pass01_desc = previous_desc;
    g_pass01_render_context = previous_context;
    g_pass01_node = previous_node;
}

void __fastcall hook_pass0d(
    void *node,
    void *render_context,
    void *pass_desc) noexcept
{
    ++g_pass0d_calls;
    g_pass0d_last_thread.store(
        static_cast<std::uint32_t>(GetCurrentThreadId()),
        std::memory_order_relaxed);

    void *const previous_context = g_pass0d_render_context;
    const unsigned previous_depth = g_pass0d_depth;

    g_pass0d_render_context = render_context;
    g_pass0d_depth = previous_depth + 1u;

    if (g_pass0d_site.original != nullptr)
        g_pass0d_site.original(node, render_context, pass_desc);

    g_pass0d_depth = previous_depth;
    g_pass0d_render_context = previous_context;
}

bool build_trampoline(hook_site &site) noexcept
{
    auto *mem = static_cast<std::uint8_t *>(
        VirtualAlloc(
            nullptr,
            64u,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
    if (mem == nullptr)
        return false;

    std::size_t cursor = 0u;
    std::memcpy(
        mem + cursor,
        site.original_bytes.data(),
        site.original_bytes.size());
    cursor += site.original_bytes.size();

    mem[cursor++] = 0xFFu;
    mem[cursor++] = 0x25u;
    const std::uint32_t zero = 0u;
    std::memcpy(
        mem + cursor,
        &zero,
        sizeof(zero));
    cursor += sizeof(zero);

    const std::uint64_t resume =
        static_cast<std::uint64_t>(
            g_base + site.rva + k_stolen);
    std::memcpy(
        mem + cursor,
        &resume,
        sizeof(resume));
    cursor += sizeof(resume);

    if (FlushInstructionCache(
            GetCurrentProcess(),
            mem,
            cursor) == FALSE) {
        VirtualFree(mem, 0u, MEM_RELEASE);
        return false;
    }

    site.trampoline = mem;
    site.original =
        reinterpret_cast<pass_fn>(site.trampoline);
    return true;
}

bool patch_entry(
    hook_site &site,
    const void *hook) noexcept
{
    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base + site.rva);

    if (!process_memory::safe_read_bytes(
            target,
            site.original_bytes.data(),
            site.original_bytes.size()) ||
        site.original_bytes != k_expected)
        return false;

    if (!build_trampoline(site))
        return false;

    std::array<std::uint8_t, k_stolen> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;

    const std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));

    const std::uint64_t destination =
        reinterpret_cast<std::uint64_t>(hook);
    std::memcpy(
        patch.data() + 6u,
        &destination,
        sizeof(destination));

    site.target = target;
    if (!write_bytes(
            site.target,
            patch.data(),
            patch.size())) {
        site.target = nullptr;
        return false;
    }

    site.patched = true;
    return true;
}

void restore_site(hook_site &site) noexcept
{
    if (site.patched &&
        site.target != nullptr)
        (void)write_bytes(
            site.target,
            site.original_bytes.data(),
            site.original_bytes.size());

    site.patched = false;
    site.target = nullptr;
    site.original = nullptr;

    if (site.trampoline != nullptr) {
        VirtualFree(
            site.trampoline,
            0u,
            MEM_RELEASE);
        site.trampoline = nullptr;
    }
}

bool read_image_state(
    std::uintptr_t &image_state) noexcept
{
    image_state = 0u;
    if (g_base == 0u)
        return false;

    return process_memory::safe_read_bytes(
        reinterpret_cast<const void *>(
            g_base + k_rva_image_state_global),
        &image_state,
        sizeof(image_state)) &&
        image_state != 0u;
}

bool read_resource_id(
    std::uintptr_t image_state,
    std::size_t offset,
    std::uint32_t &resource_id) noexcept
{
    resource_id = 0u;
    return image_state != 0u &&
        process_memory::safe_read_bytes(
            reinterpret_cast<const void *>(
                image_state + offset),
            &resource_id,
            sizeof(resource_id)) &&
        resource_id != 0u;
}

bool classify_first_pass01(
    operators::dof::flat_mode &mode,
    std::uint32_t &support_resource) noexcept
{
    mode = operators::dof::flat_mode::unknown;
    support_resource = 0u;

    if (g_pass01_depth == 0u ||
        g_pass01_desc == nullptr ||
        g_pass01_render_context == nullptr)
        return false;

    // Exact retail ImageProcessDof_Flat pass01 routing at 0x455960:
    //   original source = [pass_desc + 0x0C]
    //   authoritative source = [image_state + 0x78]
    //   support t1 = [image_state + 0x88] normally, or +0xC0 when
    //                render_context[0x24B8] selects the alternate support.
    // Do not require unrelated +0x250/+0x230 resources: retail pass01 does not.
    std::uint32_t original_source = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass01_desc) + 0x0Cu,
            &original_source,
            sizeof(original_source)) ||
        original_source == 0u)
        return false;

    std::uintptr_t image_state = 0u;
    if (!read_image_state(image_state))
        return false;

    std::uint32_t retail_source = 0u;
    if (!read_resource_id(
            image_state,
            0x78u,
            retail_source) ||
        original_source != retail_source)
        return false;

    std::uint8_t alternate_support = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass01_render_context) + 0x24B8u,
            &alternate_support,
            sizeof(alternate_support)))
        return false;

    const std::size_t support_offset =
        alternate_support != 0u
            ? 0xC0u
            : 0x88u;
    if (!read_resource_id(
            image_state,
            support_offset,
            support_resource))
        return false;

    mode =
        alternate_support != 0u
            ? operators::dof::flat_mode::alternate
            : operators::dof::flat_mode::primary;
    return true;
}

bool validate_scene_srv(
    ID3D11ShaderResourceView *srv) noexcept
{
    if (srv == nullptr)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    srv->GetDesc(&view);
    if (view.ViewDimension !=
            D3D11_SRV_DIMENSION_TEXTURE2D)
        return false;

    ID3D11Resource *resource = nullptr;
    srv->GetResource(&resource);
    if (resource == nullptr)
        return false;

    ID3D11Texture2D *texture = nullptr;
    const HRESULT query =
        resource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&texture));
    resource->Release();

    if (FAILED(query) || texture == nullptr)
        return false;

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    texture->Release();

    return
        desc.Width != 0u &&
        desc.Height != 0u &&
        desc.ArraySize == 1u &&
        desc.SampleDesc.Count == 1u &&
        desc.SampleDesc.Quality == 0u;
}

bool validate_depth_srv(
    ID3D11ShaderResourceView *srv) noexcept
{
    if (srv == nullptr)
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    srv->GetDesc(&view);

    if (view.ViewDimension !=
            D3D11_SRV_DIMENSION_TEXTURE2D ||
        view.Format !=
            DXGI_FORMAT_R24_UNORM_X8_TYPELESS)
        return false;

    ID3D11Resource *resource = nullptr;
    srv->GetResource(&resource);
    if (resource == nullptr)
        return false;

    ID3D11Texture2D *texture = nullptr;
    const HRESULT query =
        resource->QueryInterface(
            __uuidof(ID3D11Texture2D),
            reinterpret_cast<void **>(&texture));
    resource->Release();

    if (FAILED(query) ||
        texture == nullptr)
        return false;

    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    texture->Release();

    return
        desc.Width != 0u &&
        desc.Height != 0u &&
        desc.MipLevels == 1u &&
        desc.ArraySize == 1u &&
        desc.Format == DXGI_FORMAT_R24G8_TYPELESS &&
        desc.SampleDesc.Count == 1u &&
        desc.SampleDesc.Quality == 0u;
}

bool acquire_scene_without_cache_drift(
    ID3D11DeviceContext *context,
    std::uint32_t source_resource,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;
    *out = nullptr;

    if (context == nullptr ||
        source_resource == 0u ||
        g_pass01_render_context == nullptr ||
        g_base == 0u)
        return false;

    // Retail srv_bind indexes current resource IDs at
    // render_context + 0xD4 + slot*4. Slot 0 is therefore +0xD4.
    std::uint32_t previous_resource = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass01_render_context) + 0xD4u,
            &previous_resource,
            sizeof(previous_resource)))
        return false;

    const auto bind =
        reinterpret_cast<srv_bind_fn>(
            g_base + k_rva_srv_bind);

    bind(
        g_pass01_render_context,
        0u,
        source_resource);

    ID3D11ShaderResourceView *srv = nullptr;
    context->PSGetShaderResources(
        0u,
        1u,
        &srv);

    bind(
        g_pass01_render_context,
        0u,
        previous_resource);

    if (srv == nullptr)
        return false;

    if (!validate_scene_srv(srv)) {
        srv->Release();
        ++g_abi_reject;
        return false;
    }

    *out = srv;
    return true;
}

bool acquire_support_without_cache_drift(
    ID3D11DeviceContext *context,
    std::uint32_t support_resource,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;
    *out = nullptr;

    if (context == nullptr ||
        support_resource == 0u ||
        g_pass01_render_context == nullptr ||
        g_base == 0u)
        return false;

    std::uint32_t previous_resource = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass01_render_context) + 0xD8u,
            &previous_resource,
            sizeof(previous_resource)))
        return false;

    const auto bind =
        reinterpret_cast<srv_bind_fn>(
            g_base + k_rva_srv_bind);

    bind(
        g_pass01_render_context,
        1u,
        support_resource);

    ID3D11ShaderResourceView *srv = nullptr;
    context->PSGetShaderResources(
        1u,
        1u,
        &srv);

    bind(
        g_pass01_render_context,
        1u,
        previous_resource);

    if (srv == nullptr)
        return false;

    if (!validate_depth_srv(srv)) {
        srv->Release();
        ++g_abi_reject;
        return false;
    }

    *out = srv;
    return true;
}

} // namespace

bool register_host_depth_route_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_pass01_site.patched &&
        g_pass0d_site.patched)
        return true;

    g_base = process_memory::image_base();
    if (g_base == 0u ||
        !patch_entry(
            g_pass01_site,
            reinterpret_cast<const void *>(
                &hook_pass01)) ||
        !patch_entry(
            g_pass0d_site,
            reinterpret_cast<const void *>(
                &hook_pass0d))) {
        restore_site(g_pass0d_site);
        restore_site(g_pass01_site);
        g_base = 0u;
        return false;
    }

    g_pass01_calls.store(0u);
    g_pass0d_calls.store(0u);
    g_first_pass01_hits.store(0u);
    g_mode_primary.store(0u);
    g_mode_alternate.store(0u);
    g_source_capture_ok.store(0u);
    g_source_capture_fail.store(0u);
    g_support_capture_ok.store(0u);
    g_support_capture_fail.store(0u);
    g_abi_reject.store(0u);
    g_pass01_last_thread.store(0u);
    g_pass0d_last_thread.store(0u);

    return true;
}

void unregister_host_depth_route_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_pass00_restore.active)
        (void)pop_host_pass00_state();

    restore_site(g_pass0d_site);
    restore_site(g_pass01_site);

    g_base = 0u;
    g_pass01_node = nullptr;
    g_pass01_render_context = nullptr;
    g_pass01_desc = nullptr;
    g_pass01_depth = 0u;
    g_pass0d_render_context = nullptr;
    g_pass0d_depth = 0u;
    g_pass00_restore = {};
}

bool capture_host_dof_inputs(
    ID3D11DeviceContext *context,
    host_dof_inputs &out) noexcept
{
    release_host_dof_inputs(out);

    if (context == nullptr ||
        g_pass01_depth == 0u ||
        g_pass01_node == nullptr ||
        g_pass01_render_context == nullptr ||
        g_pass01_desc == nullptr ||
        !g_pass01_site.patched) {
        ++g_source_capture_fail;
        return false;
    }

    operators::dof::flat_mode mode =
        operators::dof::flat_mode::unknown;
    std::uint32_t support_resource = 0u;
    if (!classify_first_pass01(
            mode,
            support_resource)) {
        ++g_source_capture_fail;
        return false;
    }

    ++g_first_pass01_hits;
    if (mode == operators::dof::flat_mode::primary)
        ++g_mode_primary;
    else
        ++g_mode_alternate;

    // pass_desc+0x0C is the exact host source resource ID for this
    // ImageProcessDof_Flat pass. Do not infer scene color from the currently
    // bound retained-shader t0: runtime proved that r0 t0 is the depth SRV.
    std::uint32_t source_resource = 0u;
    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                g_pass01_desc) + 0x0Cu,
            &source_resource,
            sizeof(source_resource)) ||
        source_resource == 0u) {
        ++g_source_capture_fail;
        return false;
    }

    ID3D11ShaderResourceView *source = nullptr;
    if (!acquire_scene_without_cache_drift(
            context,
            source_resource,
            &source)) {
        ++g_source_capture_fail;
        return false;
    }

    ++g_source_capture_ok;

    ID3D11ShaderResourceView *support = nullptr;
    if (!acquire_support_without_cache_drift(
            context,
            support_resource,
            &support)) {
        source->Release();
        ++g_support_capture_fail;
        return false;
    }

    ++g_support_capture_ok;

    const auto variant =
        reinterpret_cast<pass00_variant_fn>(
            g_base + k_rva_pass00_variant);

    out.source_68 = source;
    out.dofrate_support_t1 = support;
    out.mode = mode;
    out.pass00_fragment0 =
        variant(g_pass01_node);
    out.ready = true;
    return true;
}

void release_host_dof_inputs(
    host_dof_inputs &inputs) noexcept
{
    if (inputs.dofrate_support_t1 != nullptr)
        inputs.dofrate_support_t1->Release();
    if (inputs.source_68 != nullptr)
        inputs.source_68->Release();
    inputs = {};
}

bool push_host_pass00_state() noexcept
{
    if (g_pass00_restore.active ||
        g_pass01_depth == 0u ||
        g_pass01_render_context == nullptr ||
        g_base == 0u)
        return false;

    operators::dof::flat_mode mode =
        operators::dof::flat_mode::unknown;
    std::uint32_t support = 0u;
    if (!classify_first_pass01(
            mode,
            support))
        return false;

    pass00_restore_state saved{};
    saved.render_context =
        g_pass01_render_context;

    if (!process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                saved.render_context) + 0x2280u,
            &saved.filter_token,
            sizeof(saved.filter_token)) ||
        !process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                saved.render_context) + 0x90u,
            &saved.aux_object,
            sizeof(saved.aux_object)) ||
        saved.aux_object == nullptr ||
        !process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                saved.aux_object) + 0x23Cu,
            &saved.aux_edx,
            sizeof(saved.aux_edx)) ||
        !process_memory::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                saved.aux_object) + 0x238u,
            &saved.aux_r8,
            sizeof(saved.aux_r8)))
        return false;

    const auto aux =
        reinterpret_cast<aux_state_fn>(
            g_base + k_rva_aux_state);
    const auto filter =
        reinterpret_cast<filter_state_fn>(
            g_base + k_rva_filter_state);

    aux(
        saved.aux_object,
        0u,
        0u);
    filter(
        saved.render_context,
        3u);

    saved.active = true;
    g_pass00_restore = saved;
    return true;
}

bool pop_host_pass00_state() noexcept
{
    if (!g_pass00_restore.active ||
        g_base == 0u)
        return false;

    const auto saved =
        g_pass00_restore;
    g_pass00_restore = {};

    const auto filter =
        reinterpret_cast<filter_state_fn>(
            g_base + k_rva_filter_state);
    const auto aux =
        reinterpret_cast<aux_state_fn>(
            g_base + k_rva_aux_state);

    filter(
        saved.render_context,
        saved.filter_token);
    aux(
        saved.aux_object,
        saved.aux_edx,
        saved.aux_r8);

    return true;
}

bool inside_exact_dof_pass01() noexcept
{
    return
        g_pass01_depth != 0u &&
        g_pass01_node != nullptr &&
        g_pass01_render_context != nullptr &&
        g_pass01_desc != nullptr;
}

bool inside_exact_dof_pass0d() noexcept
{
    return
        g_pass0d_depth != 0u &&
        g_pass0d_render_context != nullptr;
}

host_depth_route_telemetry host_depth_route_status() noexcept
{
    return {
        g_pass01_calls.load(),
        g_pass0d_calls.load(),
        g_first_pass01_hits.load(),
        g_mode_primary.load(),
        g_mode_alternate.load(),
        g_source_capture_ok.load(),
        g_source_capture_fail.load(),
        g_support_capture_ok.load(),
        g_support_capture_fail.load(),
        g_abi_reject.load(),
        g_pass01_last_thread.load(),
        g_pass0d_last_thread.load(),
        g_pass01_site.patched &&
            g_pass01_site.original != nullptr,
        g_pass0d_site.patched &&
            g_pass0d_site.original != nullptr
    };
}

} // namespace dsrrl::runtime::dof
