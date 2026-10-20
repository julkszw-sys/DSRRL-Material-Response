#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "dsrrl/runtime/texture_identity_transport.hpp"
#include <reshade.hpp>

#include <Windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
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
extern "C" void dsrrl_gi_reflection_branch_entry();

extern "C" {
void *g_dsrrl_texture_name_resume = nullptr;
void *g_dsrrl_texture_name_clear_resume = nullptr;
void *g_dsrrl_gi_reflection_resume = nullptr;
void *g_dsrrl_gi_reflection_format_name = nullptr;
void *g_dsrrl_gi_reflection_format_call = nullptr;
}

namespace dsrrl::runtime::texture_identity_transport {
namespace {

constexpr char k_exe_sha256[] =
    "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b";

constexpr std::uintptr_t k_name_rva = 0x583AA6u;
constexpr std::uintptr_t k_clear_rva = 0x583E81u;
constexpr std::uintptr_t k_gi_reflection_branch_rva = 0x2017F4u;
constexpr std::uintptr_t k_gi_reflection_format_rva = 0x12E9D40u;
constexpr std::uintptr_t k_gi_reflection_formatter_rva = 0x0CF290u;
// Only r14d==5 enters this 17-byte format-call sequence.
constexpr std::array<std::uint8_t,17> k_gi_reflection_branch_bytes = {
  0x44,0x89,0x7C,0x24,0x20,0x48,0x8D,0x15,0x40,0x85,0x0E,0x01,
  0xE8,0x8B,0xDA,0xEC,0xFF};

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
hook g_name{}, g_clear{}, g_gi_reflection{};
std::atomic<std::uint64_t> g_gi_reflection_branch_hits{0u};
hook_status g_status{};
constexpr std::size_t k_logical_name_capacity = 512u;
thread_local std::array<wchar_t,k_logical_name_capacity + 1u>
    g_logical_name{};
thread_local std::size_t g_logical_name_length = 0u;

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
    g_logical_name_length = 0u;
    g_logical_name[0] = L'\0';

    if (logical_name == nullptr)
        return;

    const auto begin =
        reinterpret_cast<std::uintptr_t>(
            logical_name);
    auto cursor = begin;
    std::size_t copied = 0u;

    while (copied < k_logical_name_capacity) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi))
            return;

        if (mbi.State != MEM_COMMIT ||
            (mbi.Protect & PAGE_GUARD) != 0u)
            return;

        const DWORD access =
            mbi.Protect & 0xffu;
        const bool readable =
            access == PAGE_READONLY ||
            access == PAGE_READWRITE ||
            access == PAGE_WRITECOPY ||
            access == PAGE_EXECUTE_READ ||
            access == PAGE_EXECUTE_READWRITE ||
            access == PAGE_EXECUTE_WRITECOPY;
        if (!readable)
            return;

        const auto region_end =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress) +
            mbi.RegionSize;
        if (region_end <= cursor)
            return;

        const auto available_bytes =
            region_end - cursor;
        const auto available_chars =
            static_cast<std::size_t>(
                available_bytes /
                sizeof(wchar_t));
        const auto remaining =
            k_logical_name_capacity -
            copied;
        const auto scan =
            std::min(
                available_chars,
                remaining);
        if (scan == 0u)
            return;

        const auto *src =
            reinterpret_cast<const wchar_t *>(
                cursor);

        for (std::size_t i = 0u;
             i < scan;
             ++i) {
            const wchar_t ch = src[i];
            if (ch == L'\0') {
                g_logical_name[copied] =
                    L'\0';
                g_logical_name_length =
                    copied;
                return;
            }

            g_logical_name[copied++] = ch;
        }

        cursor +=
            scan * sizeof(wchar_t);
    }

    // No terminator within the bounded identity window: fail open.
    g_logical_name_length = 0u;
    g_logical_name[0] = L'\0';
}

} // namespace

extern "C" void dsrrl_gi_reflection_branch_observer(
    std::uint32_t packed_subarea, std::uint32_t local_id,
    const wchar_t *mapstem) noexcept
{
    const auto count = g_gi_reflection_branch_hits.fetch_add(
        1u, std::memory_order_relaxed) + 1u;
    if (count > 12u && count % 65536u != 0u)
        return;
    char observed_stem[40]{};
    bool stem_exact=false;
    if (mapstem!=nullptr) {
        for(std::size_t i=0u;i<sizeof(observed_stem)-1u;++i) {
            if(!readable_range(mapstem+i,sizeof(wchar_t))) break;
            const auto ch=mapstem[i];
            if(ch==L'\0') {stem_exact=true;break;}
            if((ch>=L'A'&&ch<=L'Z')||(ch>=L'a'&&ch<=L'z')||
               (ch>=L'0'&&ch<=L'9')||ch==L'_'||ch==L'-')
                observed_stem[i]=static_cast<char>(ch);
            else break;
        }
    }
    char line[350]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V230 GI REFLECTION PRODUCER] stage=RETAIL_FORMAT_BRANCH"
        " class=GI_Reflection selector=5 subarea=%u local=%u packed=%u"
        " mapstem=%.39s stem_exact=%u"
        " hit=%llu source_to_native_join=NOT_ESTABLISHED"
        " action=OBSERVE_ONLY_NO_ALIAS",
        packed_subarea,local_id,
        packed_subarea*10000u+local_id,
        observed_stem,stem_exact?1u:0u,
        static_cast<unsigned long long>(count));
    reshade::log::message(reshade::log::level::warning,line);
}

extern "C" void
dsrrl_texture_name_observer(
    const wchar_t *logical_name) noexcept
{
    capture_name(logical_name);
}

extern "C" void
dsrrl_texture_name_clear_observer() noexcept
{
    g_logical_name_length = 0u;
    g_logical_name[0] = L'\0';
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
#if defined(DSRRL_V230_GI_REFLECTION_SOURCE_BRANCH)
    // Separate guarded observational hook: failure cannot revoke the existing
    // exact texture identity producer, and cannot authorize any GPU probe.
    if (prepare_hook(g_gi_reflection,k_gi_reflection_branch_rva,
          k_gi_reflection_branch_bytes,
          reinterpret_cast<void *>(&dsrrl_gi_reflection_branch_entry))) {
        g_dsrrl_gi_reflection_resume = reinterpret_cast<std::uint8_t *>(
            g_gi_reflection.target)+g_gi_reflection.stolen;
        g_dsrrl_gi_reflection_format_name = reinterpret_cast<void *>(
            g_base+k_gi_reflection_format_rva);
        g_dsrrl_gi_reflection_format_call = reinterpret_cast<void *>(
            g_base+k_gi_reflection_formatter_rva);
        g_status.gi_reflection_branch_armed=arm(g_gi_reflection);
    }
    reshade::log::message(reshade::log::level::warning,
      g_status.gi_reflection_branch_armed
        ? "[DSRRL V230 GI REFLECTION SOURCE HOOK] status=ARMED exact_exe=1 branch_rva=0x2017f4 selector=5 purpose=DIAG_ONLY"
        : "[DSRRL V230 GI REFLECTION SOURCE HOOK] status=FAIL_OPEN branch_byte_or_patch_mismatch");
#endif
    return true;

fail:
    uninstall();
    return false;
}

void uninstall() noexcept
{
    const bool gi_ok = restore(g_gi_reflection);
    const bool clear_ok =
        restore(g_clear);
    const bool name_ok =
        restore(g_name);

    if (!gi_ok || !clear_ok || !name_ok) {
        g_status.restore_failed = true;
        g_status.gi_reflection_branch_armed = g_gi_reflection.patched;
        g_status.name_hook_armed =
            g_name.patched;
        g_status.clear_hook_armed =
            g_clear.patched;
        return;
    }

    g_dsrrl_gi_reflection_resume = nullptr;
    g_dsrrl_gi_reflection_format_name = nullptr;
    g_dsrrl_gi_reflection_format_call = nullptr;
    g_dsrrl_texture_name_resume = nullptr;
    g_dsrrl_texture_name_clear_resume = nullptr;
    g_logical_name_length = 0u;
    g_logical_name[0] = L'\0';
    g_base = 0u;
    g_status = {};
}

hook_status status() noexcept
{
    return g_status;
}

bool snapshot_raw(
    const wchar_t *&logical_name,
    std::size_t &length) noexcept
{
    logical_name = nullptr;
    length = 0u;

    if (!g_status.name_hook_armed ||
        !g_status.clear_hook_armed ||
        g_logical_name_length == 0u)
        return false;

    logical_name = g_logical_name.data();
    length = g_logical_name_length;
    return true;
}

bool snapshot(
    std::wstring &logical_name) noexcept
{
    logical_name.clear();

    if (!g_status.name_hook_armed ||
        !g_status.clear_hook_armed ||
        g_logical_name_length == 0u)
        return false;

    try {
        logical_name.assign(
            g_logical_name.data(),
            g_logical_name_length);
        return !logical_name.empty();
    } catch (...) {
        logical_name.clear();
        return false;
    }
}

} // namespace dsrrl::runtime::texture_identity_transport
