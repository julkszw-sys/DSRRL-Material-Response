#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/engine_hooks.hpp"
#include "dsrrl/sha256.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <string_view>

extern "C" void selector_hook_entry();
extern "C" void texture_name_hook_entry();
extern "C" void texture_name_clear_hook_entry();

extern "C" {
void *g_selector_trampoline = nullptr;
void *g_texture_name_resume = nullptr;
void *g_texture_name_clear_resume = nullptr;
}

namespace dsrrl::runtime::engine {
namespace {

constexpr std::string_view k_exe_sha256 =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";
constexpr std::string_view k_binder_sha256 =
    "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394";

constexpr std::uintptr_t k_rva_selector = 0x22BA20;
constexpr std::uintptr_t k_rva_flver_parse = 0x20D910;
constexpr std::uintptr_t k_rva_mtd_parse = 0x295ED0;
constexpr std::uintptr_t k_rva_texture_name = 0x583AA6;
constexpr std::uintptr_t k_rva_texture_name_clear = 0x583E81;

constexpr std::array<std::uint8_t,16> k_flver_parse_bytes = {
    0x48,0x89,0x5C,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57
};
constexpr std::array<std::uint8_t,15> k_selector_bytes = {
    0x40,0x53,0x48,0x83,0xEC,0x30,0x49,0x63,0xC0,0x45,0x8B,0xD1,0x48,0x8B,0xDA
};
constexpr std::array<std::uint8_t,15> k_mtd_bytes = {
    0x40,0x57,0x48,0x83,0xEC,0x40,0x48,0xC7,0x44,0x24,0x20,0xFE,0xFF,0xFF,0xFF
};
// Retail a45aaa... exact bytes at 0x140583AA6..0x140583AB3.
// The MASM detour replays these instructions and resumes at 0x140583AB4.
constexpr std::array<std::uint8_t,14> k_texture_name_bytes = {
    0x4C,0x8D,0x75,0xE7,0x48,0x83,0x7D,0xFF,0x08,0x4C,0x0F,0x43,0x75,0xE7
};
// Retail a45aaa... exact bytes at 0x140583E81..0x140583E8F.
// The MASM detour replays them after the callback and resumes at 0x140583E90.
constexpr std::array<std::uint8_t,15> k_texture_name_clear_bytes = {
    0x48,0x8B,0x9C,0x24,0xF0,0x00,0x00,0x00,0x48,0x81,0xC4,0xA0,0x00,0x00,0x00
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
hook g_selector_hook{}, g_flver_parse_hook{}, g_mtd_hook{}, g_texture_name_hook{}, g_texture_name_clear_hook{};
selector_callback g_selector_cb = nullptr;
mtd_callback g_mtd_cb = nullptr;
flver_parse_callback g_flver_parse_cb = nullptr;
texture_name_callback g_texture_name_cb = nullptr;
texture_clear_callback g_texture_clear_cb = nullptr;

using flver_parse_fn = void(__fastcall *)(void *, const void *);
flver_parse_fn g_flver_parse_original = nullptr;

using mtd_parse_fn = void(__fastcall *)(void *, const void *, std::uint32_t, const wchar_t *);
mtd_parse_fn g_mtd_original = nullptr;

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

    h.target = target;
    h.trampoline = tr;
    h.detour = detour;
    h.stolen = N;
    std::copy(got.begin(), got.end(), h.original.begin());
    return true;
}

bool patch_hook(hook &h) noexcept
{
    if (!h.target || !h.trampoline || !h.detour || h.stolen < 14 || h.stolen > h.original.size())
        return false;

    std::array<std::uint8_t,32> patch{};
    patch.fill(0x90);
    patch[0] = 0xFF; patch[1] = 0x25;
    std::uint32_t zero = 0; std::memcpy(patch.data() + 2, &zero, 4);
    const std::uint64_t dest = reinterpret_cast<std::uint64_t>(h.detour);
    std::memcpy(patch.data() + 6, &dest, 8);
    h.patched = true; // Track possible mutation even if instruction-cache flush fails.
    return write_bytes(h.target, patch.data(), h.stolen);
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
    h.target = nullptr;
    h.detour = nullptr;
    h.stolen = 0;
}

void clear_callbacks() noexcept
{
    g_selector_cb = nullptr;
    g_mtd_cb = nullptr;
    g_flver_parse_cb = nullptr;
    g_texture_name_cb = nullptr;
    g_texture_clear_cb = nullptr;
}

void __fastcall flver_parse_hook_entry(void *model, const void *raw) noexcept
{
    // Retail 0x14020D910 receives the parsed-model destination in RCX and the
    // still-raw FLVER2 image in RDX. Observe before the original mutates the
    // image by rebasing its internal offsets.
    if (g_flver_parse_cb) g_flver_parse_cb(model, raw);
    if (g_flver_parse_original) g_flver_parse_original(model, raw);
}

void __fastcall mtd_hook_entry(
    void *material,
    const void *raw,
    std::uint32_t len,
    const wchar_t *semantic_key) noexcept
{
    // Retail 0x140295ED0 receives the exact MTD BND entry name in R9.
    // Preserve that discriminator for exact (semantic name, raw SHA) routing;
    // the original parser still receives the untouched pointer afterward.
    if (g_mtd_cb) g_mtd_cb(material, raw, len, semantic_key);
    if (g_mtd_original) g_mtd_original(material, raw, len, semantic_key);
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

bool install(
    selector_callback selector,
    mtd_callback mtd,
    flver_parse_callback flver_parse,
    texture_name_callback texture_name,
    texture_clear_callback texture_clear) noexcept
{
    if (!g_base || g_selector_hook.patched || g_flver_parse_hook.patched || g_mtd_hook.patched ||
        g_texture_name_hook.patched || g_texture_name_clear_hook.patched)
        return false;

    g_selector_cb = selector;
    g_mtd_cb = mtd;
    g_flver_parse_cb = flver_parse;
    g_texture_name_cb = texture_name;
    g_texture_clear_cb = texture_clear;

    if (!prepare_hook(g_flver_parse_hook, k_rva_flver_parse, k_flver_parse_bytes,
                      reinterpret_cast<void *>(&flver_parse_hook_entry)))
        goto fail;
    g_flver_parse_original = reinterpret_cast<flver_parse_fn>(g_flver_parse_hook.trampoline);

    if (!prepare_hook(g_selector_hook, k_rva_selector, k_selector_bytes,
                      reinterpret_cast<void *>(&selector_hook_entry)))
        goto fail;
    g_selector_trampoline = g_selector_hook.trampoline;

    if (!prepare_hook(g_mtd_hook, k_rva_mtd_parse, k_mtd_bytes,
                      reinterpret_cast<void *>(&mtd_hook_entry)))
        goto fail;
    g_mtd_original = reinterpret_cast<mtd_parse_fn>(g_mtd_hook.trampoline);

    if (!prepare_hook(g_texture_name_hook, k_rva_texture_name, k_texture_name_bytes,
                      reinterpret_cast<void *>(&texture_name_hook_entry)))
        goto fail;
    g_texture_name_resume = static_cast<std::uint8_t *>(g_texture_name_hook.target) + g_texture_name_hook.stolen;

    if (!prepare_hook(g_texture_name_clear_hook, k_rva_texture_name_clear, k_texture_name_clear_bytes,
                      reinterpret_cast<void *>(&texture_name_clear_hook_entry)))
        goto fail;
    g_texture_name_clear_resume =
        static_cast<std::uint8_t *>(g_texture_name_clear_hook.target) + g_texture_name_clear_hook.stolen;

    // Patch the passive/resource hooks first, then the material hooks. Any fault
    // restores all already-patched sites below, so partial activation is impossible.
    if (!patch_hook(g_texture_name_hook) ||
        !patch_hook(g_flver_parse_hook) ||
        !patch_hook(g_texture_name_clear_hook) ||
        !patch_hook(g_mtd_hook) ||
        !patch_hook(g_selector_hook))
        goto fail;

    return true;

fail:
    uninstall();
    return false;
}

void uninstall() noexcept
{
    restore_hook(g_selector_hook);
    restore_hook(g_flver_parse_hook);
    restore_hook(g_mtd_hook);
    restore_hook(g_texture_name_hook);
    restore_hook(g_texture_name_clear_hook);

    g_selector_trampoline = nullptr;
    g_texture_name_resume = nullptr;
    g_texture_name_clear_resume = nullptr;
    g_mtd_original = nullptr;
    g_flver_parse_original = nullptr;
    clear_callbacks();
}

std::uintptr_t image_base() noexcept { return g_base; }

extern "C" void selector_observer(
    void *container, void *owner, void *ret, void *r14, void *r15, std::int32_t material_index) noexcept
{
    if (g_selector_cb)
        g_selector_cb(container, owner, ret, r14, r15, material_index);
}

extern "C" void texture_name_observer(const wchar_t *logical_name) noexcept
{
    if (g_texture_name_cb)
        g_texture_name_cb(logical_name);
}

extern "C" void texture_name_clear_observer() noexcept
{
    if (g_texture_clear_cb)
        g_texture_clear_cb();
}

} // namespace dsrrl::runtime::engine
