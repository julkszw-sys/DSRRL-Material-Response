#pragma once

// Offline-verified DSR source fingerprint matching for compressed textures.
// Pure and inactive: this header does NOT change a GPU resource, draw, or SRV.
// Valid input requires complete *real* D3D11 initial texture subresources.
// The digest covers only tightly packed compressed mip bytes, matching
// original TPF DDS payload SHA256 independently of producer row padding.

#include "dsrrl/operators/legacy_plan/sha256_bytes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace dsrrl::runtime::stock_dsr_compressed_identity {

using digest = dsrrl::operators::legacy_plan::hashing::sha256_digest;

struct texture2d_descriptor {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t mip_levels = 0;
    std::uint32_t array_size = 0;
    std::uint32_t sample_count = 0;
    std::uint32_t dxgi_format = 0;
    bool shader_resource = false;
};

struct mip_initial_data {
    const std::uint8_t *data = nullptr;
    std::uint32_t row_pitch = 0;
    std::uint32_t slice_pitch = 0;
};

inline std::optional<digest> exact_full_mip_digest(
    const texture2d_descriptor &desc,
    const mip_initial_data *subresources,
    std::size_t subresource_count) noexcept
{
    // The accepted carrier is an ordinary single-layer, single-sample
    // texture, with the precise stock DDS format. Conversion to a different
    // format, a sparse/partial upload or missing mip must fail open.
    if (!desc.shader_resource ||
        desc.array_size != 1u ||
        desc.sample_count != 1u ||
        desc.width == 0u || desc.height == 0u ||
        desc.width > 16384u || desc.height > 16384u ||
        desc.mip_levels == 0u || desc.mip_levels > 16u ||
        subresources == nullptr ||
        subresource_count != desc.mip_levels)
        return std::nullopt;

    const std::uint32_t block_size =
        desc.dxgi_format == 71u ? 8u :
        (desc.dxgi_format == 77u || desc.dxgi_format == 83u)
            ? 16u : 0u;
    if (block_size == 0u)
        return std::nullopt;

    dsrrl::operators::legacy_plan::hashing::detail::sha256_context sha{};

    for (std::uint32_t level = 0; level < desc.mip_levels; ++level) {
        const auto &src = subresources[level];
        const std::uint32_t mip_width =
            (desc.width >> level) ? (desc.width >> level) : 1u;
        const std::uint32_t mip_height =
            (desc.height >> level) ? (desc.height >> level) : 1u;
        const std::uint32_t rows = (mip_height + 3u) / 4u;
        const std::uint32_t blocks = (mip_width + 3u) / 4u;
        const std::uint32_t tight_row = blocks * block_size;

        if (src.data == nullptr || src.row_pitch < tight_row ||
            src.slice_pitch == 0u ||
            static_cast<std::uint64_t>(src.row_pitch) * rows >
                src.slice_pitch)
            return std::nullopt;

        // Reject implausibly large CPU read requests; not a content-identity
        // fallback or a partial hashing mode.
        if (static_cast<std::uint64_t>(src.row_pitch) * rows >
            std::numeric_limits<std::uint32_t>::max())
            return std::nullopt;

        for (std::uint32_t row = 0; row < rows; ++row) {
            sha.update(
                src.data + static_cast<std::size_t>(row) * src.row_pitch,
                tight_row);
        }
    }
    return sha.finish();
}

} // namespace dsrrl::runtime::stock_dsr_compressed_identity
