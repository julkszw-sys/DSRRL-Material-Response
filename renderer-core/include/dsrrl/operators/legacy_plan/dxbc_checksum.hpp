#pragma once

#include <cstddef>
#include <cstdint>

namespace dsrrl::operators::legacy_plan::dxbc {

inline std::uint32_t read_u32(const std::uint8_t *p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8u) |
           (static_cast<std::uint32_t>(p[2]) << 16u) |
           (static_cast<std::uint32_t>(p[3]) << 24u);
}

inline void write_u32(std::uint8_t *p, std::uint32_t value) noexcept
{
    p[0] = static_cast<std::uint8_t>(value);
    p[1] = static_cast<std::uint8_t>(value >> 8u);
    p[2] = static_cast<std::uint8_t>(value >> 16u);
    p[3] = static_cast<std::uint8_t>(value >> 24u);
}

inline bool checksum_container_valid(
    const std::uint8_t *bytes,
    std::size_t size) noexcept
{
    if (bytes == nullptr || size < 0x20u)
        return false;

    if (bytes[0] != 'D' || bytes[1] != 'X' ||
        bytes[2] != 'B' || bytes[3] != 'C')
        return false;

    return static_cast<std::size_t>(read_u32(bytes + 24u)) == size;
}

namespace detail {

inline std::uint32_t rol(std::uint32_t value, std::uint32_t shift) noexcept
{
    return (value << shift) | (value >> (32u - shift));
}

inline constexpr std::uint32_t k_md5[64] = {
    0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,
    0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
    0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,
    0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
    0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,
    0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
    0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,
    0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
    0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,
    0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
    0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,
    0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
    0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,
    0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
    0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,
    0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u
};

inline constexpr std::uint8_t k_shift[64] = {
    7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
    5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
    4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
    6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
};

inline void transform(
    std::uint32_t state[4],
    const std::uint8_t *block) noexcept
{
    std::uint32_t words[16]{};
    for (std::uint32_t i = 0; i < 16u; ++i)
        words[i] = read_u32(block + i * 4u);

    std::uint32_t a = state[0];
    std::uint32_t b = state[1];
    std::uint32_t c = state[2];
    std::uint32_t d = state[3];

    const std::uint32_t aa = a;
    const std::uint32_t bb = b;
    const std::uint32_t cc = c;
    const std::uint32_t dd = d;

    for (std::uint32_t i = 0; i < 64u; ++i) {
        std::uint32_t f = 0;
        std::uint32_t g = 0;

        if (i < 16u) {
            f = (b & c) | ((~b) & d);
            g = i;
        } else if (i < 32u) {
            f = (d & b) | ((~d) & c);
            g = (5u * i + 1u) % 16u;
        } else if (i < 48u) {
            f = b ^ c ^ d;
            g = (3u * i + 5u) % 16u;
        } else {
            f = c ^ (b | (~d));
            g = (7u * i) % 16u;
        }

        const std::uint32_t old_d = d;
        d = c;
        c = b;
        b = b + rol(
            a + f + k_md5[i] + words[g],
            k_shift[i]);
        a = old_d;
    }

    state[0] = aa + a;
    state[1] = bb + b;
    state[2] = cc + c;
    state[3] = dd + d;
}

} // namespace detail

// Recomputes the legacy DXBC container checksum used by the audited P2.2
// materializer. The checksum field itself occupies bytes 4..19 and is excluded
// from the hashed payload by starting at byte 0x14.
inline bool fix_checksum(
    std::uint8_t *bytes,
    std::size_t size) noexcept
{
    if (!checksum_container_valid(bytes, size))
        return false;

    const std::uint8_t *payload = bytes + 0x14u;
    const std::size_t payload_size = size - 0x14u;
    const std::uint64_t bit_count =
        static_cast<std::uint64_t>(payload_size) * 8ull;

    std::uint32_t state[4] = {
        0x67452301u,
        0xefcdab89u,
        0x98badcfeu,
        0x10325476u
    };

    const std::size_t full = payload_size & ~std::size_t{63u};
    for (std::size_t offset = 0; offset < full; offset += 64u)
        detail::transform(state, payload + offset);

    const std::size_t tail = payload_size - full;
    std::uint8_t block[64]{};

    if (tail >= 56u) {
        for (std::size_t i = 0; i < tail; ++i)
            block[i] = payload[full + i];

        block[tail] = 0x80u;
        detail::transform(state, block);

        for (auto &value : block)
            value = 0u;

        write_u32(block, static_cast<std::uint32_t>(bit_count));
        write_u32(
            block + 60u,
            static_cast<std::uint32_t>((bit_count >> 2u) | 1ull));
        detail::transform(state, block);
    } else {
        write_u32(block, static_cast<std::uint32_t>(bit_count));

        for (std::size_t i = 0; i < tail; ++i)
            block[4u + i] = payload[full + i];

        block[4u + tail] = 0x80u;
        write_u32(
            block + 60u,
            static_cast<std::uint32_t>((bit_count >> 2u) | 1ull));
        detail::transform(state, block);
    }

    for (std::uint32_t i = 0; i < 4u; ++i)
        write_u32(bytes + 4u + i * 4u, state[i]);

    return true;
}

} // namespace dsrrl::operators::legacy_plan::dxbc
