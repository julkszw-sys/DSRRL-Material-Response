#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/texture_identity_transport.hpp"

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib,"bcrypt.lib")

extern "C" void dsrrl_texture_name_hook_entry();
extern "C" void dsrrl_texture_name_clear_hook_entry();

extern "C" {
void *g_dsrrl_texture_name_resume = nullptr;
void *g_dsrrl_texture_name_clear_resume = nullptr;
}

namespace dsrrl::runtime::texture_identity_transport {
namespace {

constexpr char k_exe_sha256[] =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";

constexpr std::uintptr_t k_name_rva = 0x583AA6u;
constexpr std::uintptr_t k_clear_rva = 0x583E81u;

constexpr std::array<std::uint8_t,14> k_name_bytes = {
    0x4C,0x8D,0x75,0xE7,0x48,0x83,0x7D,0xFF,0x08,0x4C,0x0F,0x43,0x75,0xE7
};

constexpr std::array<std::uint8_t,15> k_clear_bytes = {
    0x48,0x8B,0x9C,0x24,0xF0,0x00,0x00,0x00,0x48,0x81,0xC4,0xA0,0x00,0x00,0x00
};

struct hook {
    void *target = nullptr;
    void *detour = nullptr;
    std::size_t stolen = 0;
    std::array<std::uint8_t,32> original{};
    bool patched = false;
};

std::uintptr_t g_base = 0;
hook g_name{}, g_clear{};
hook_status g_status{};
thread_local std::wstring g_logical_name;

bool readable_range(
    const void *ptr,
    std::size_t size) noexcept
{
    if (ptr == nullptr)
        return false;
    if (size == 0u)
        return true;

    auto cursor =
        reinterpret_cast<std::uintptr_t>(ptr);
    const auto end = cursor + size;
    if (end < cursor)
        return false;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi))
            return false;

        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & PAGE_GUARD) != 0u)
            return false;

        const DWORD access = mbi.Protect & 0xffu;
        const bool readable =
            access == PAGE_READONLY ||
            access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY ||
            access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE ||
            access == PAGE_EXECUTE_WRITECOPY;

        if (!readable)
            return false;

        const auto region_begin =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress);
        const auto region_end =
            region_begin + mbi.RegionSize;

        if (region_end <= cursor ||
            region_end < region_begin)
            return false;

        cursor =
            region_end < end
                ? region_end
                : end;
    }

    return true;
}

bool write_bytes(
    void *dst,
    const void *src,
    std::size_t size) noexcept
{
    DWORD old = 0;
    if (!VirtualProtect(
            dst,
            size,
            PAGE_EXECUTE_READWRITE,
            &old))
        return false;

    std::memcpy(dst, src, size);

    const bool flushed =
        FlushInstructionCache(
            GetCurrentProcess(),
            dst,
            size) != FALSE;

    DWORD ignored = 0;
    (void)VirtualProtect(
        dst,
        size,
        old,
        &ignored);

    return flushed;
}

template <std::size_t N>
bool prepare_hook(
    hook &out,
    std::uintptr_t rva,
    const std::array<std::uint8_t,N> &expected,
    void *detour) noexcept
{
    static_assert(N >= 14u && N <= 32u);

    auto *target =
        reinterpret_cast<std::uint8_t *>(
            g_base + rva);

    if (!readable_range(target, N) ||
        std::memcmp(
            target,
            expected.data(),
            N) != 0)
        return false;

    out.target = target;
    out.detour = detour;
    out.stolen = N;
    std::copy(
        expected.begin(),
        expected.end(),
        out.original.begin());
    return true;
}

bool arm(hook &h) noexcept
{
    if (h.target == nullptr ||
        h.detour == nullptr ||
        h.stolen < 14u ||
        h.stolen > h.original.size())
        return false;

    std::array<std::uint8_t,32> patch{};
    patch.fill(0x90u);
    patch[0] = 0xFFu;
    patch[1] = 0x25u;

    std::uint32_t zero = 0u;
    std::memcpy(
        patch.data() + 2u,
        &zero,
        sizeof(zero));

    const auto dest =
        reinterpret_cast<std::uint64_t>(
            h.detour);
    std::memcpy(
        patch.data() + 6u,
        &dest,
        sizeof(dest));

    h.patched = true;
    return write_bytes(
        h.target,
        patch.data(),
        h.stolen);
}

bool restore(hook &h) noexcept
{
    if (!h.patched) {
        h = {};
        return true;
    }

    if (h.target == nullptr ||
        h.stolen == 0u ||
        !write_bytes(
            h.target,
            h.original.data(),
            h.stolen))
        return false;

    if (std::memcmp(
            h.target,
            h.original.data(),
            h.stolen) != 0)
        return false;

    h = {};
    return true;
}

std::wstring process_path()
{
    std::wstring path(32768, L'\0');
    const DWORD count =
        GetModuleFileNameW(
            nullptr,
            path.data(),
            static_cast<DWORD>(path.size()));

    if (count == 0u ||
        count >= path.size())
        return {};

    path.resize(count);
    return path;
}

bool exe_matches() noexcept
{
    const auto path = process_path();
    if (path.empty())
        return false;

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    DWORD object_length = 0u;
    DWORD hash_length = 0u;
    DWORD copied = 0u;

    std::array<std::uint8_t,32> digest{};
    bool ok = false;

    if (BCryptOpenAlgorithmProvider(
            &alg,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0u) < 0)
        return false;

    if (BCryptGetProperty(
            alg,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(
                &object_length),
            sizeof(object_length),
            &copied,
            0u) >= 0 &&
        BCryptGetProperty(
            alg,
            BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(
                &hash_length),
            sizeof(hash_length),
            &copied,
            0u) >= 0 &&
        hash_length == digest.size()) {
        std::vector<std::uint8_t> object(
            object_length);

        if (BCryptCreateHash(
                alg,
                &hash,
                object.data(),
                object_length,
                nullptr,
                0u,
                0u) >= 0) {
            std::ifstream stream(
                path,
                std::ios::binary);
            std::array<char,65536> buffer{};

            ok = !!stream;
            while (ok && stream) {
                stream.read(
                    buffer.data(),
                    buffer.size());
                const auto n =
                    stream.gcount();

                if (n > 0 &&
                    BCryptHashData(
                        hash,
                        reinterpret_cast<PUCHAR>(
                            buffer.data()),
                        static_cast<ULONG>(n),
                        0u) < 0)
                    ok = false;
            }

            if (ok)
                ok =
                    BCryptFinishHash(
                        hash,
                        digest.data(),
                        static_cast<ULONG>(
                            digest.size()),
                        0u) >= 0;
        }
    }

    if (hash != nullptr)
        BCryptDestroyHash(hash);
    if (alg != nullptr)
        BCryptCloseAlgorithmProvider(
            alg,
            0u);

    if (!ok)
        return false;

    static constexpr char hex[] =
        "0123456789abcdef";

    std::string actual(64u, '0');
    for (std::size_t i = 0;
         i < digest.size();
         ++i) {
        actual[i * 2u] =
            hex[digest[i] >> 4u];
        actual[i * 2u + 1u] =
            hex[digest[i] & 0x0Fu];
    }

    return actual == k_exe_sha256;
}

void capture_name(
    const wchar_t *logical_name) noexcept
{
    g_logical_name.clear();

    if (logical_name == nullptr)
        return;

    try {
        constexpr std::size_t k_max = 512u;
        bool terminated = false;

        for (std::size_t i = 0;
             i < k_max;
             ++i) {
            wchar_t ch = L'\0';

            if (!readable_range(
                    logical_name + i,
                    sizeof(ch)))
                return;

            std::memcpy(
                &ch,
                logical_name + i,
                sizeof(ch));

            if (ch == L'\0') {
                terminated = true;
                break;
            }

            g_logical_name.push_back(ch);
        }

        if (!terminated)
            g_logical_name.clear();
    } catch (...) {
        g_logical_name.clear();
    }
}

} // namespace

extern "C" void
dsrrl_texture_name_observer(
    const wchar_t *logical_name) noexcept
{
    capture_name(logical_name);
}

extern "C" void
dsrrl_texture_name_clear_observer() noexcept
{
    g_logical_name.clear();
}

bool install() noexcept
{
    if (g_name.patched ||
        g_clear.patched)
        return false;

    g_status = {};

    if (!exe_matches())
        return false;

    g_base =
        reinterpret_cast<std::uintptr_t>(
            GetModuleHandleW(nullptr));
    if (g_base == 0u)
        return false;

    g_status.provenance_ok = true;

    if (!prepare_hook(
            g_name,
            k_name_rva,
            k_name_bytes,
            reinterpret_cast<void *>(
                &dsrrl_texture_name_hook_entry)) ||
        !prepare_hook(
            g_clear,
            k_clear_rva,
            k_clear_bytes,
            reinterpret_cast<void *>(
                &dsrrl_texture_name_clear_hook_entry)))
        goto fail;

    g_dsrrl_texture_name_resume =
        reinterpret_cast<std::uint8_t *>(
            g_name.target) +
        g_name.stolen;

    g_dsrrl_texture_name_clear_resume =
        reinterpret_cast<std::uint8_t *>(
            g_clear.target) +
        g_clear.stolen;

    if (!arm(g_name) ||
        !arm(g_clear))
        goto fail;

    g_status.name_hook_armed = true;
    g_status.clear_hook_armed = true;
    return true;

fail:
    uninstall();
    return false;
}

void uninstall() noexcept
{
    const bool clear_ok =
        restore(g_clear);
    const bool name_ok =
        restore(g_name);

    if (!clear_ok || !name_ok) {
        g_status.restore_failed = true;
        g_status.name_hook_armed =
            g_name.patched;
        g_status.clear_hook_armed =
            g_clear.patched;
        return;
    }

    g_dsrrl_texture_name_resume = nullptr;
    g_dsrrl_texture_name_clear_resume = nullptr;
    g_logical_name.clear();
    g_base = 0u;
    g_status = {};
}

hook_status status() noexcept
{
    return g_status;
}

bool snapshot(
    std::wstring &logical_name) noexcept
{
    logical_name.clear();

    if (!g_status.name_hook_armed ||
        !g_status.clear_hook_armed ||
        g_logical_name.empty())
        return false;

    try {
        logical_name = g_logical_name;
        return !logical_name.empty();
    } catch (...) {
        logical_name.clear();
        return false;
    }
}

} // namespace dsrrl::runtime::texture_identity_transport
