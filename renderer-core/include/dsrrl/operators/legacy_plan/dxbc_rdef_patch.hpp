#pragma once

#include "dsrrl/operators/legacy_plan/dxbc_checksum.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <vector>

namespace dsrrl::operators::legacy_plan::dxbc::rdef {

inline bool append_constant_buffer_binding(
    std::vector<std::uint8_t> &payload,
    std::string_view name,
    std::uint32_t bind_point,
    std::uint32_t byte_size) noexcept
{
    constexpr std::size_t k_cb_desc_size = 24u;
    constexpr std::size_t k_resource_desc_size = 32u;

    if (payload.size() < 32u ||
        name.empty() ||
        bind_point >= 14u ||
        byte_size == 0u ||
        (byte_size & 15u) != 0u)
        return false;

    const std::uint32_t cb_count =
        read_u32(payload.data());
    const std::uint32_t cb_offset =
        read_u32(payload.data() + 4u);
    const std::uint32_t resource_count =
        read_u32(payload.data() + 8u);
    const std::uint32_t resource_offset =
        read_u32(payload.data() + 12u);

    if (cb_count == 0u ||
        cb_count > 64u ||
        resource_count == 0u ||
        resource_count > 256u)
        return false;

    const std::size_t cb_bytes =
        static_cast<std::size_t>(cb_count) *
        k_cb_desc_size;
    const std::size_t resource_bytes =
        static_cast<std::size_t>(resource_count) *
        k_resource_desc_size;

    if (cb_offset > payload.size() ||
        cb_bytes > payload.size() - cb_offset ||
        resource_offset > payload.size() ||
        resource_bytes > payload.size() - resource_offset)
        return false;

    // Constant-buffer namespace is independent from SRV/sampler namespaces.
    // Only reject another constant-buffer resource that overlaps this slot.
    for (std::uint32_t i = 0u;
         i < resource_count;
         ++i) {
        const std::size_t base =
            static_cast<std::size_t>(resource_offset) +
            static_cast<std::size_t>(i) *
                k_resource_desc_size;

        const std::uint32_t type =
            read_u32(payload.data() + base + 4u);
        const std::uint32_t existing_bind =
            read_u32(payload.data() + base + 20u);
        const std::uint32_t existing_count =
            read_u32(payload.data() + base + 24u);

        if (type == 0u &&
            existing_count != 0u &&
            existing_bind <= bind_point &&
            bind_point < existing_bind + existing_count)
            return false;
    }

    try {
        while ((payload.size() & 3u) != 0u)
            payload.push_back(0u);

        if (payload.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        const std::uint32_t name_offset =
            static_cast<std::uint32_t>(payload.size());

        payload.insert(
            payload.end(),
            reinterpret_cast<const std::uint8_t *>(
                name.data()),
            reinterpret_cast<const std::uint8_t *>(
                name.data()) + name.size());
        payload.push_back(0u);

        while ((payload.size() & 3u) != 0u)
            payload.push_back(0u);

        if (payload.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        const std::uint32_t new_cb_offset =
            static_cast<std::uint32_t>(payload.size());

        std::vector<std::uint8_t> cb_copy(
            payload.data() + cb_offset,
            payload.data() + cb_offset + cb_bytes);

        payload.insert(
            payload.end(),
            cb_copy.begin(),
            cb_copy.end());

        const std::size_t cb_new =
            payload.size();
        payload.resize(
            cb_new + k_cb_desc_size);

        write_u32(
            payload.data() + cb_new + 0u,
            name_offset);
        write_u32(
            payload.data() + cb_new + 4u,
            0u);
        write_u32(
            payload.data() + cb_new + 8u,
            0u);
        write_u32(
            payload.data() + cb_new + 12u,
            byte_size);
        write_u32(
            payload.data() + cb_new + 16u,
            0u);
        write_u32(
            payload.data() + cb_new + 20u,
            0u);

        if (payload.size() >
            std::numeric_limits<std::uint32_t>::max())
            return false;

        const std::uint32_t new_resource_offset =
            static_cast<std::uint32_t>(payload.size());

        std::vector<std::uint8_t> resource_copy(
            payload.data() + resource_offset,
            payload.data() + resource_offset +
                resource_bytes);

        payload.insert(
            payload.end(),
            resource_copy.begin(),
            resource_copy.end());

        const std::size_t resource_new =
            payload.size();
        payload.resize(
            resource_new +
            k_resource_desc_size);

        write_u32(
            payload.data() + resource_new + 0u,
            name_offset);
        write_u32(
            payload.data() + resource_new + 4u,
            0u);
        write_u32(
            payload.data() + resource_new + 8u,
            0u);
        write_u32(
            payload.data() + resource_new + 12u,
            0u);
        write_u32(
            payload.data() + resource_new + 16u,
            0u);
        write_u32(
            payload.data() + resource_new + 20u,
            bind_point);
        write_u32(
            payload.data() + resource_new + 24u,
            1u);
        write_u32(
            payload.data() + resource_new + 28u,
            0u);

        write_u32(
            payload.data() + 0u,
            cb_count + 1u);
        write_u32(
            payload.data() + 4u,
            new_cb_offset);
        write_u32(
            payload.data() + 8u,
            resource_count + 1u);
        write_u32(
            payload.data() + 12u,
            new_resource_offset);
    } catch (...) {
        return false;
    }

    return true;
}

inline bool has_constant_buffer_binding(
    const std::vector<std::uint8_t> &payload,
    std::uint32_t bind_point) noexcept
{
    constexpr std::size_t k_resource_desc_size = 32u;

    if (payload.size() < 32u)
        return false;

    const std::uint32_t resource_count =
        read_u32(payload.data() + 8u);
    const std::uint32_t resource_offset =
        read_u32(payload.data() + 12u);

    if (resource_count == 0u ||
        resource_count > 256u)
        return false;

    const std::size_t resource_bytes =
        static_cast<std::size_t>(resource_count) *
        k_resource_desc_size;

    if (resource_offset > payload.size() ||
        resource_bytes > payload.size() - resource_offset)
        return false;

    for (std::uint32_t i = 0u;
         i < resource_count;
         ++i) {
        const std::size_t base =
            static_cast<std::size_t>(resource_offset) +
            static_cast<std::size_t>(i) *
                k_resource_desc_size;

        if (read_u32(payload.data() + base + 4u) != 0u)
            continue;

        const std::uint32_t existing_bind =
            read_u32(payload.data() + base + 20u);
        const std::uint32_t existing_count =
            read_u32(payload.data() + base + 24u);

        if (existing_count != 0u &&
            existing_bind <= bind_point &&
            bind_point < existing_bind + existing_count)
            return true;
    }

    return false;
}

} // namespace dsrrl::operators::legacy_plan::dxbc::rdef
