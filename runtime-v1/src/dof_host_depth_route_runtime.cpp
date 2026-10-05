#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/dof_host_depth_route_runtime.hpp"

#include "dsrrl/runtime/engine_hooks.hpp"

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

constexpr std::uintptr_t k_rva_pass0d = 0x00456460u;
constexpr std::uintptr_t k_rva_srv_bind = 0x000BADC0u;
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

using pass0d_fn = void(__fastcall *)(void *, void *, void *);
using srv_bind_fn = void(__fastcall *)(void *, std::uint32_t, std::uint32_t);

std::mutex g_mutex;
std::uintptr_t g_base = 0u;
void *g_target = nullptr;
void *g_trampoline = nullptr;
pass0d_fn g_original = nullptr;
std::array<std::uint8_t, k_stolen> g_original_bytes{};
bool g_patched = false;

thread_local void *g_render_context = nullptr;
thread_local unsigned g_pass0d_depth = 0u;

std::atomic<std::uint64_t> g_pass0d_calls{0u};
std::atomic<std::uint64_t> g_tls_hits{0u};
std::atomic<std::uint64_t> g_selector_88{0u};
std::atomic<std::uint64_t> g_selector_c0{0u};
std::atomic<std::uint64_t> g_bind_ok{0u};
std::atomic<std::uint64_t> g_bind_fail{0u};
std::atomic<std::uint64_t> g_abi_reject{0u};

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

void __fastcall hook_pass0d(
    void *node,
    void *render_context,
    void *pass_desc) noexcept
{
    ++g_pass0d_calls;

    void *const previous_context = g_render_context;
    const unsigned previous_depth = g_pass0d_depth;

    g_render_context = render_context;
    g_pass0d_depth = previous_depth + 1u;

    if (g_original != nullptr)
        g_original(node, render_context, pass_desc);

    g_pass0d_depth = previous_depth;
    g_render_context = previous_context;
}

bool build_trampoline() noexcept
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
        g_original_bytes.data(),
        g_original_bytes.size());
    cursor += g_original_bytes.size();

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
            g_base + k_rva_pass0d + k_stolen);
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

    g_trampoline = mem;
    g_original =
        reinterpret_cast<pass0d_fn>(g_trampoline);
    return true;
}

bool patch_entry() noexcept
{
    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base + k_rva_pass0d);

    if (!engine::safe_read_bytes(
            target,
            g_original_bytes.data(),
            g_original_bytes.size()) ||
        g_original_bytes != k_expected)
        return false;

    if (!build_trampoline())
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

    const std::uint64_t hook =
        reinterpret_cast<std::uint64_t>(
            &hook_pass0d);
    std::memcpy(
        patch.data() + 6u,
        &hook,
        sizeof(hook));

    g_target = target;
    if (!write_bytes(
            g_target,
            patch.data(),
            patch.size())) {
        g_target = nullptr;
        return false;
    }

    g_patched = true;
    return true;
}

void restore_locked() noexcept
{
    if (g_patched &&
        g_target != nullptr)
        (void)write_bytes(
            g_target,
            g_original_bytes.data(),
            g_original_bytes.size());

    g_patched = false;
    g_target = nullptr;
    g_original = nullptr;

    if (g_trampoline != nullptr) {
        VirtualFree(
            g_trampoline,
            0u,
            MEM_RELEASE);
        g_trampoline = nullptr;
    }
}

bool read_depth_route(
    void *render_context,
    std::uint32_t &resource_id,
    bool &alternate) noexcept
{
    resource_id = 0u;
    alternate = false;

    if (render_context == nullptr ||
        g_base == 0u)
        return false;

    std::uint8_t flag = 0u;
    if (!engine::safe_read_bytes(
            static_cast<const std::uint8_t *>(
                render_context) + 0x24B8u,
            &flag,
            sizeof(flag)))
        return false;

    std::uintptr_t image_state = 0u;
    const auto *global =
        reinterpret_cast<const void *>(
            g_base + k_rva_image_state_global);
    if (!engine::safe_read_bytes(
            global,
            &image_state,
            sizeof(image_state)) ||
        image_state == 0u)
        return false;

    alternate = flag != 0u;
    const std::size_t offset =
        alternate ? 0xC0u : 0x88u;

    return engine::safe_read_bytes(
        reinterpret_cast<const void *>(
            image_state + offset),
        &resource_id,
        sizeof(resource_id)) &&
        resource_id != 0u;
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
            reinterpret_cast<void **>(
                &texture));
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

} // namespace

bool register_host_depth_route_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);

    if (g_patched)
        return true;

    g_base = engine::image_base();
    if (g_base == 0u ||
        !patch_entry()) {
        restore_locked();
        g_base = 0u;
        return false;
    }

    g_pass0d_calls.store(0u);
    g_tls_hits.store(0u);
    g_selector_88.store(0u);
    g_selector_c0.store(0u);
    g_bind_ok.store(0u);
    g_bind_fail.store(0u);
    g_abi_reject.store(0u);

    return true;
}

void unregister_host_depth_route_runtime() noexcept
{
    std::lock_guard<std::mutex> lock(g_mutex);
    restore_locked();
    g_base = 0u;
    g_render_context = nullptr;
    g_pass0d_depth = 0u;
}

bool acquire_host_depth_support_t1(
    ID3D11DeviceContext *context,
    ID3D11ShaderResourceView **out) noexcept
{
    if (out == nullptr)
        return false;
    *out = nullptr;

    if (context == nullptr ||
        g_pass0d_depth == 0u ||
        g_render_context == nullptr ||
        g_base == 0u ||
        !g_patched) {
        ++g_bind_fail;
        return false;
    }

    ++g_tls_hits;

    std::uint32_t resource_id = 0u;
    bool alternate = false;
    if (!read_depth_route(
            g_render_context,
            resource_id,
            alternate)) {
        ++g_bind_fail;
        return false;
    }

    if (alternate)
        ++g_selector_c0;
    else
        ++g_selector_88;

    const auto bind =
        reinterpret_cast<srv_bind_fn>(
            g_base + k_rva_srv_bind);

    bind(
        g_render_context,
        1u,
        resource_id);

    ID3D11ShaderResourceView *srv = nullptr;
    context->PSGetShaderResources(
        1u,
        1u,
        &srv);

    if (srv == nullptr) {
        ++g_bind_fail;
        return false;
    }

    if (!validate_depth_srv(srv)) {
        srv->Release();
        ++g_abi_reject;
        ++g_bind_fail;
        return false;
    }

    *out = srv;
    ++g_bind_ok;
    return true;
}

bool inside_exact_dof_pass0d() noexcept
{
    return
        g_pass0d_depth != 0u &&
        g_render_context != nullptr;
}

host_depth_route_telemetry host_depth_route_status() noexcept
{
    return {
        g_pass0d_calls.load(),
        g_tls_hits.load(),
        g_selector_88.load(),
        g_selector_c0.load(),
        g_bind_ok.load(),
        g_bind_fail.load(),
        g_abi_reject.load(),
        g_patched && g_original != nullptr
    };
}

} // namespace dsrrl::runtime::dof
