#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::dof::process_memory {

struct safe_read_profile_snapshot {
    std::uint64_t sampled_calls = 0u;
    std::uint64_t sampled_virtual_queries = 0u;
    std::uint64_t sampled_failures = 0u;
    std::uint64_t sampled_bytes = 0u;
};

inline constexpr std::uint32_t k_safe_read_profile_sample_period = 1024u;
inline std::atomic<std::uint64_t> g_safe_read_profile_sampled_calls{0u};
inline std::atomic<std::uint64_t> g_safe_read_profile_sampled_virtual_queries{0u};
inline std::atomic<std::uint64_t> g_safe_read_profile_sampled_failures{0u};
inline std::atomic<std::uint64_t> g_safe_read_profile_sampled_bytes{0u};
inline thread_local std::uint32_t g_safe_read_profile_sequence = 0u;

inline safe_read_profile_snapshot safe_read_profile() noexcept
{
    return {
        g_safe_read_profile_sampled_calls.load(std::memory_order_relaxed),
        g_safe_read_profile_sampled_virtual_queries.load(std::memory_order_relaxed),
        g_safe_read_profile_sampled_failures.load(std::memory_order_relaxed),
        g_safe_read_profile_sampled_bytes.load(std::memory_order_relaxed)
    };
}


inline std::uintptr_t image_base() noexcept
{
    return reinterpret_cast<std::uintptr_t>(
        GetModuleHandleW(nullptr));
}

inline bool readable_protection(DWORD protect) noexcept
{
    if ((protect & PAGE_GUARD) != 0u ||
        (protect & PAGE_NOACCESS) != 0u)
        return false;

    switch (protect & 0xFFu) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

inline bool safe_read_bytes(
    const void *source,
    void *destination,
    std::size_t size) noexcept
{
    const auto profile_sequence =
        ++g_safe_read_profile_sequence;
    const bool profile_sample =
        (profile_sequence &
         (k_safe_read_profile_sample_period - 1u)) == 0u;

    if (profile_sample) {
        g_safe_read_profile_sampled_calls.fetch_add(
            1u,
            std::memory_order_relaxed);
        g_safe_read_profile_sampled_bytes.fetch_add(
            static_cast<std::uint64_t>(size),
            std::memory_order_relaxed);
    }

    const auto fail = [profile_sample]() noexcept {
        if (profile_sample)
            g_safe_read_profile_sampled_failures.fetch_add(
                1u,
                std::memory_order_relaxed);
        return false;
    };

    if (source == nullptr ||
        destination == nullptr ||
        size == 0u)
        return fail();

    auto cursor =
        reinterpret_cast<std::uintptr_t>(source);
    const auto end = cursor + size;
    if (end < cursor)
        return fail();

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (profile_sample)
            g_safe_read_profile_sampled_virtual_queries.fetch_add(
                1u,
                std::memory_order_relaxed);
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi) ||
            mbi.State != MEM_COMMIT ||
            !readable_protection(mbi.Protect))
            return fail();

        const auto region_end =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress) +
            mbi.RegionSize;
        if (region_end <= cursor)
            return fail();

        cursor = region_end < end
            ? region_end
            : end;
    }

    std::memcpy(destination, source, size);
    return true;
}

} // namespace dsrrl::runtime::dof::process_memory
