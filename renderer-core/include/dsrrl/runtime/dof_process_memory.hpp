#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace dsrrl::runtime::dof::process_memory {

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
    if (source == nullptr ||
        destination == nullptr ||
        size == 0u)
        return false;

    auto cursor =
        reinterpret_cast<std::uintptr_t>(source);
    const auto end = cursor + size;
    if (end < cursor)
        return false;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION mbi{};
        if (VirtualQuery(
                reinterpret_cast<const void *>(cursor),
                &mbi,
                sizeof(mbi)) != sizeof(mbi) ||
            mbi.State != MEM_COMMIT ||
            !readable_protection(mbi.Protect))
            return false;

        const auto region_end =
            reinterpret_cast<std::uintptr_t>(
                mbi.BaseAddress) +
            mbi.RegionSize;
        if (region_end <= cursor)
            return false;

        cursor = region_end < end
            ? region_end
            : end;
    }

    std::memcpy(destination, source, size);
    return true;
}

} // namespace dsrrl::runtime::dof::process_memory
