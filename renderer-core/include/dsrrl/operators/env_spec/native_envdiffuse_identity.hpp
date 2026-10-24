#pragma once

// Independent stock DSR EnvDiffuse (t11/t13) identity authority.
// This file only parses exact fingerprints; it does not authorize a shader
// replacement or imply a PTDE-visible pixel match.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

namespace dsrrl::operators::env_spec {

inline constexpr char k_native_envdiffuse_atlas_relative_path[] =
    "DSRRL\\EnvSpec\\PackedGI\\DSR_NATIVE_ENVDIFFUSE_PROBE_HASH_V1.bin";
inline constexpr std::size_t k_native_envdiffuse_count = 342u;
inline constexpr std::size_t k_native_envdiffuse_stride = 36u;
inline constexpr std::size_t k_native_envdiffuse_atlas_size =
    24u + k_native_envdiffuse_count * k_native_envdiffuse_stride;
inline constexpr std::array<std::uint8_t, 32> k_native_envdiffuse_atlas_sha = {{
    0xcdu,0x35u,0x49u,0xeeu,0x84u,0x41u,0x0fu,0x93u,
    0x29u,0x09u,0xe8u,0x18u,0xf0u,0x7du,0x50u,0x09u,
    0x34u,0x73u,0x14u,0xf9u,0x91u,0xbfu,0x6bu,0x0cu,
    0x69u,0xbfu,0xfau,0x46u,0xfcu,0xebu,0x74u,0x7cu
}};
struct native_diffuse_hash_record {
    std::array<std::uint8_t, 32> sha{};
    std::uint16_t ordinal = 0u;
};
using native_diffuse_hash_atlas =
    std::array<native_diffuse_hash_record, k_native_envdiffuse_count>;

inline bool parse_native_diffuse_hash_atlas(
    const std::uint8_t *data, std::size_t size,
    native_diffuse_hash_atlas &out) noexcept
{
    out = {};
    if (data == nullptr || size != k_native_envdiffuse_atlas_size ||
        std::memcmp(data, "DSREND01", 8u) != 0)
        return false;
    auto u32 = [data](std::size_t o) noexcept -> std::uint32_t {
        return static_cast<std::uint32_t>(data[o]) |
            (static_cast<std::uint32_t>(data[o+1u]) << 8u) |
            (static_cast<std::uint32_t>(data[o+2u]) << 16u) |
            (static_cast<std::uint32_t>(data[o+3u]) << 24u);
    };
    if (u32(8u) != 1u || u32(12u) != k_native_envdiffuse_count ||
        u32(16u) != k_native_envdiffuse_stride || u32(20u) != 0u)
        return false;
    std::array<bool, k_native_envdiffuse_count> visited{};
    for (std::size_t i = 0u; i < k_native_envdiffuse_count; ++i) {
        const std::size_t off = 24u + i*k_native_envdiffuse_stride;
        const unsigned id = static_cast<unsigned>(data[off+32u]) |
            (static_cast<unsigned>(data[off+33u]) << 8u);
        if (id >= k_native_envdiffuse_count ||
            data[off+34u] != 0u || data[off+35u] != 0u ||
            visited[id] || id != i)
            return false;
        visited[id] = true;
        std::memcpy(out[i].sha.data(), data+off, 32u);
        out[i].ordinal = static_cast<std::uint16_t>(id);
    }
    return true;
}

// Two original DSR EnvDiffuse cubes (m17 ordinals 321/322) are byte-equal.
// A digest collision is ambiguity, NEVER authorization to choose first match.
inline std::optional<std::uint16_t> unique_native_diffuse_probe(
    const native_diffuse_hash_atlas &atlas,
    const std::array<std::uint8_t, 32> &sha) noexcept
{
    std::optional<std::uint16_t> found;
    for (const auto &entry : atlas) {
        if (entry.sha != sha)
            continue;
        if (found.has_value())
            return std::nullopt;
        found = entry.ordinal;
    }
    return found;
}
} // namespace dsrrl::operators::env_spec
