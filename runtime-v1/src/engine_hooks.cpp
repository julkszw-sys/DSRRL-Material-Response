#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/sha256.hpp"

#include <Windows.h>
#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#endif

#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string_view>

extern "C" void selector_hook_entry();
extern "C" { void *g_selector_trampoline = nullptr; }

namespace dsrrl::runtime::engine {
namespace {

constexpr std::string_view k_exe_sha256 =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::string_view k_binder_sha256 =
    "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394";

constexpr std::uintptr_t k_rva_selector = 0x22BA20;
constexpr std::uintptr_t k_rva_mtd_parse = 0x295ED0;
constexpr std::uintptr_t k_rva_wrapper_type5 = 0x1C0BE0;
constexpr std::uintptr_t k_rva_wrapper_type6 = 0x1C0C10;
constexpr std::uintptr_t k_rva_true_blend = 0x5642F0;
constexpr std::uintptr_t k_rva_steady_packer = 0x563B80;

constexpr std::array<std::uint8_t,15> k_selector_bytes = {
    0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA
};
constexpr std::array<std::uint8_t,15> k_mtd_bytes = {
    0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF
};
constexpr std::array<std::uint8_t,17> k_wrapper_bytes = {
    0x48,0x83,0xEC,0x38,0x4D,0x8B,0xC8,0xF3,0x0F,0x11,0x5C,0x24,0x20,0x4C,0x8B,0x41,0x40
};
constexpr std::array<std::uint8_t,19> k_blend_bytes = {
    0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x57,0x48,0x81,0xEC,0xC0,0x00,0x00,0x00
};
constexpr std::array<std::uint8_t,14> k_steady_bytes = {
    0x48,0x89,0x5C,0x24,0x08,0x57,0x48,0x83,0xEC,0x40,0x48,0x8B,0x41,0x18
};

struct hook {
    void *target = nullptr;
    void *trampoline = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

std::uintptr_t g_base = 0;
hook g_selector_hook{}, g_mtd_hook{};
hook g_wrapper5_hook{}, g_wrapper6_hook{}, g_blend_hook{}, g_steady_hook{};
selector_callback g_selector_cb = nullptr;
mtd_callback g_mtd_cb = nullptr;
upper_lower_callbacks g_ul_callbacks{};

using mtd_parse_fn = void(__fastcall *)(void *, const void *, std::uint32_t, void *);
using wrapper_fn = void *(__fastcall *)(void *, void *, void *, float);
using blend_fn = void *(__fastcall *)(void *, const void *, const void *, float);
using steady_fn = void(__fastcall *)(void *, void *, std::int32_t);

mtd_parse_fn g_mtd_original = nullptr;
wrapper_fn g_wrapper5_original = nullptr;
wrapper_fn g_wrapper6_original = nullptr;
blend_fn g_blend_original = nullptr;
steady_fn g_steady_original = nullptr;

std::filesystem::path process_path()
{
    std::wstring buf(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
    if (n == 0 || n >= buf.size()) return {};
    buf.resize(n);
    return buf;
}

std::filesystem::path binder_path()
{
    const auto exe = process_path();
    return exe.empty() ? std::filesystem::path{} :
        exe.parent_path() / L"shader" / L"FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx";
}

bool write_bytes(void *at, const void *src, std::size_t n) noexcept
{
    DWORD old = 0;
    if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    std::memcpy(at, src, n);
    const bool flushed = FlushInstructionCache(GetCurrentProcess(), at, n) != FALSE;
    DWORD ignored = 0;
    VirtualProtect(at, n, old, &ignored);
    return flushed;
}

template<std::size_t N>
bool prepare_hook(hook &h, std::uintptr_t rva, const std::array<std::uint8_t,N> &expected, void *detour) noexcept
{
    static_assert(N >= 14 && N <= 32);
    auto *target = reinterpret_cast<std::uint8_t *>(g_base + rva);
    std::array<std::uint8_t,N> got{};
    if (!safe_read_bytes(target, got.data(), got.size()) || got != expected) return false;

    void *tr = VirtualAlloc(nullptr, N + 14, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!tr) return false;

    std::memcpy(tr, target, N);
    auto *tail = static_cast<std::uint8_t *>(tr) + N;
    tail[0] = 0xFF; tail[1] = 0x25;
    std::uint32_t zero = 0; std::memcpy(tail + 2, &zero, 4);
    const std::uint64_t back = reinterpret_cast<std::uint64_t>(target + N);
    std::memcpy(tail + 6, &back, 8);
    FlushInstructionCache(GetCurrentProcess(), tr, N + 14);

    h.target = target; h.trampoline = tr; h.detour = detour; h.stolen = N;
    std::copy(got.begin(), got.end(), h.original.begin());
    return true;
}

bool patch_hook(hook &h) noexcept
{
    if (!h.target || !h.trampoline || !h.detour) return false;
    std::array<std::uint8_t,32> patch{}; patch.fill(0x90);
    patch[0] = 0xFF; patch[1] = 0x25;
    std::uint32_t zero = 0; std::memcpy(patch.data() + 2, &zero, 4);
    const std::uint64_t dest = reinterpret_cast<std::uint64_t>(h.detour);
    std::memcpy(patch.data() + 6, &dest, 8);
    if (!write_bytes(h.target, patch.data(), h.stolen)) return false;
    h.patched = true;
    return true;
}

void restore_hook(hook &h) noexcept
{
    if (h.patched) {
        write_bytes(h.target, h.original.data(), h.stolen);
        h.patched = false;
    }
    if (h.trampoline) {
        VirtualFree(h.trampoline, 0, MEM_RELEASE);
        h.trampoline = nullptr;
    }
    h.target = nullptr; h.detour = nullptr; h.stolen = 0;
}

void __fastcall mtd_hook_entry(void *material, const void *raw, std::uint32_t len, void *arg4) noexcept
{
    if (g_mtd_cb) g_mtd_cb(material, raw, len);
    if (g_mtd_original) g_mtd_original(material, raw, len, arg4);
}

void *run_wrapper(
    wrapper_fn original,
    void *self,
    void *owner,
    void *assignment,
    float blend) noexcept
{
    if(g_ul_callbacks.wrapper_enter)
        g_ul_callbacks.wrapper_enter(owner,assignment);

    void *result = original ? original(self,owner,assignment,blend) : nullptr;

    if(g_ul_callbacks.wrapper_exit)
        g_ul_callbacks.wrapper_exit();
    return result;
}

void *__fastcall wrapper5_hook_entry(
    void *self,void *owner,void *assignment,float blend) noexcept
{
    return run_wrapper(g_wrapper5_original,self,owner,assignment,blend);
}

void *__fastcall wrapper6_hook_entry(
    void *self,void *owner,void *assignment,float blend) noexcept
{
    return run_wrapper(g_wrapper6_original,self,owner,assignment,blend);
}

void *__fastcall blend_hook_entry(
    void *dst,const void *a,const void *b,float beta) noexcept
{
    std::uintptr_t return_rva = 0;
#if defined(_MSC_VER)
    return_rva = reinterpret_cast<std::uintptr_t>(_ReturnAddress()) - g_base;
#else
    return_rva = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)) - g_base;
#endif
    if(g_ul_callbacks.true_blend)
        g_ul_callbacks.true_blend(a,b,beta,return_rva);
    return g_blend_original ? g_blend_original(dst,a,b,beta) : dst;
}

void __fastcall steady_hook_entry(
    void *source,void *dst,std::int32_t selector) noexcept
{
    if(g_ul_callbacks.steady_cache)
        g_ul_callbacks.steady_cache(source,selector);
    if(g_steady_original)
        g_steady_original(source,dst,selector);
}

} // namespace

bool safe_read_bytes(const void *src, void *dst, std::size_t size) noexcept
{
#if defined(_MSC_VER)
    __try {
        std::memcpy(dst, src, size);
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    if (!src || !dst) return false;
    std::memcpy(dst, src, size);
    return true;
#endif
}

bool verify_provenance() noexcept
{
    try {
        const auto exe = process_path();
        const auto binder = binder_path();
        if (exe.empty() || binder.empty() || !std::filesystem::exists(binder)) return false;
        if (dsrrl::to_hex(dsrrl::sha256_file(exe)) != k_exe_sha256) return false;
        if (dsrrl::to_hex(dsrrl::sha256_file(binder)) != k_binder_sha256) return false;
        g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        return g_base != 0;
    } catch (...) {
        return false;
    }
}

bool install(selector_callback selector, mtd_callback mtd) noexcept
{
    if (!g_base || g_selector_hook.patched || g_mtd_hook.patched) return false;
    g_selector_cb = selector; g_mtd_cb = mtd;

    if (!prepare_hook(g_selector_hook, k_rva_selector, k_selector_bytes,
                      reinterpret_cast<void *>(&selector_hook_entry)))
        return false;
    g_selector_trampoline = g_selector_hook.trampoline;

    if (!prepare_hook(g_mtd_hook, k_rva_mtd_parse, k_mtd_bytes,
                      reinterpret_cast<void *>(&mtd_hook_entry))) {
        restore_hook(g_selector_hook); g_selector_trampoline = nullptr; return false;
    }
    g_mtd_original = reinterpret_cast<mtd_parse_fn>(g_mtd_hook.trampoline);

    if (!patch_hook(g_mtd_hook) || !patch_hook(g_selector_hook)) {
        uninstall(); return false;
    }
    return true;
}

bool install_upper_lower(const upper_lower_callbacks &callbacks) noexcept
{
    if(!g_base ||
       g_wrapper5_hook.target || g_wrapper6_hook.target ||
       g_blend_hook.target || g_steady_hook.target)
        return false;

    if(!callbacks.wrapper_enter || !callbacks.wrapper_exit ||
       !callbacks.steady_cache || !callbacks.true_blend)
        return false;

    g_ul_callbacks = callbacks;

    if(!prepare_hook(g_wrapper5_hook,k_rva_wrapper_type5,k_wrapper_bytes,
                     reinterpret_cast<void *>(&wrapper5_hook_entry)) ||
       !prepare_hook(g_wrapper6_hook,k_rva_wrapper_type6,k_wrapper_bytes,
                     reinterpret_cast<void *>(&wrapper6_hook_entry)) ||
       !prepare_hook(g_blend_hook,k_rva_true_blend,k_blend_bytes,
                     reinterpret_cast<void *>(&blend_hook_entry)) ||
       !prepare_hook(g_steady_hook,k_rva_steady_packer,k_steady_bytes,
                     reinterpret_cast<void *>(&steady_hook_entry))){
        uninstall_upper_lower();
        return false;
    }

    g_wrapper5_original = reinterpret_cast<wrapper_fn>(g_wrapper5_hook.trampoline);
    g_wrapper6_original = reinterpret_cast<wrapper_fn>(g_wrapper6_hook.trampoline);
    g_blend_original = reinterpret_cast<blend_fn>(g_blend_hook.trampoline);
    g_steady_original = reinterpret_cast<steady_fn>(g_steady_hook.trampoline);

    if(!patch_hook(g_wrapper5_hook) ||
       !patch_hook(g_wrapper6_hook) ||
       !patch_hook(g_blend_hook) ||
       !patch_hook(g_steady_hook)){
        uninstall_upper_lower();
        return false;
    }
    return true;
}

void uninstall_upper_lower() noexcept
{
    restore_hook(g_steady_hook);
    restore_hook(g_blend_hook);
    restore_hook(g_wrapper6_hook);
    restore_hook(g_wrapper5_hook);
    g_wrapper5_original = nullptr;
    g_wrapper6_original = nullptr;
    g_blend_original = nullptr;
    g_steady_original = nullptr;
    g_ul_callbacks = {};
}

void uninstall() noexcept
{
    uninstall_upper_lower();
    restore_hook(g_selector_hook);
    restore_hook(g_mtd_hook);
    g_selector_trampoline = nullptr;
    g_mtd_original = nullptr;
    g_selector_cb = nullptr;
    g_mtd_cb = nullptr;
}

std::uintptr_t image_base() noexcept { return g_base; }

} // namespace dsrrl::runtime::engine

extern "C" void selector_observer(
    void *container, void *owner, void *ret, void *r14, void *r15, std::int32_t material_index) noexcept
{
    // Single Core-owned selector interception. Operator islands subscribe through
    // the runtime coordinator rather than installing competing detours.
    using namespace dsrrl::runtime::engine;
    extern selector_callback dsrrl_runtime_selector_dispatch() noexcept;
    if (auto cb = dsrrl_runtime_selector_dispatch())
        cb(container, owner, ret, r14, r15, material_index);
}
