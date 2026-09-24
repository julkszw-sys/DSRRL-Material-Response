#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace dsrrl::operators::legacy_plan::hashing {

using sha256_digest = std::array<std::uint8_t, 32>;

namespace detail {

inline constexpr std::array<std::uint32_t, 64> k_sha256 = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,
    0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,
    0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,
    0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,
    0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,
    0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,
    0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,
    0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,
    0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

inline constexpr std::uint32_t rotr(
    std::uint32_t value,
    std::uint32_t shift) noexcept
{
    return (value >> shift) | (value << (32u - shift));
}

struct sha256_context {
    std::array<std::uint32_t, 8> state{
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };
    std::array<std::uint8_t, 64> buffer{};
    std::size_t used = 0;
    std::uint64_t total = 0;

    void block(const std::uint8_t *p) noexcept
    {
        std::uint32_t w[64]{};

        for (std::uint32_t i = 0; i < 16u; ++i) {
            w[i] =
                (static_cast<std::uint32_t>(p[4u * i]) << 24u) |
                (static_cast<std::uint32_t>(p[4u * i + 1u]) << 16u) |
                (static_cast<std::uint32_t>(p[4u * i + 2u]) << 8u) |
                static_cast<std::uint32_t>(p[4u * i + 3u]);
        }

        for (std::uint32_t i = 16u; i < 64u; ++i) {
            const auto s0 =
                rotr(w[i - 15u], 7u) ^
                rotr(w[i - 15u], 18u) ^
                (w[i - 15u] >> 3u);
            const auto s1 =
                rotr(w[i - 2u], 17u) ^
                rotr(w[i - 2u], 19u) ^
                (w[i - 2u] >> 10u);
            w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
        }

        auto a = state[0];
        auto b = state[1];
        auto c = state[2];
        auto d = state[3];
        auto e = state[4];
        auto f = state[5];
        auto g = state[6];
        auto h = state[7];

        for (std::uint32_t i = 0; i < 64u; ++i) {
            const auto s1 =
                rotr(e, 6u) ^ rotr(e, 11u) ^ rotr(e, 25u);
            const auto choose = (e & f) ^ ((~e) & g);
            const auto t1 = h + s1 + choose + k_sha256[i] + w[i];
            const auto s0 =
                rotr(a, 2u) ^ rotr(a, 13u) ^ rotr(a, 22u);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto t2 = s0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    void update(
        const std::uint8_t *p,
        std::size_t size) noexcept
    {
        total += size;

        while (size != 0u) {
            const auto take =
                std::min(size, buffer.size() - used);
            std::copy_n(p, take, buffer.data() + used);
            used += take;
            p += take;
            size -= take;

            if (used == buffer.size()) {
                block(buffer.data());
                used = 0;
            }
        }
    }

    sha256_digest finish() noexcept
    {
        const std::uint64_t bit_count = total * 8ull;

        buffer[used++] = 0x80u;

        if (used > 56u) {
            std::fill(buffer.begin() + used, buffer.end(), 0u);
            block(buffer.data());
            used = 0;
        }

        std::fill(
            buffer.begin() + used,
            buffer.begin() + 56u,
            0u);

        for (std::uint32_t i = 0; i < 8u; ++i)
            buffer[63u - i] =
                static_cast<std::uint8_t>(
                    bit_count >> (8u * i));

        block(buffer.data());

        sha256_digest out{};
        for (std::uint32_t i = 0; i < 8u; ++i) {
            out[4u * i] =
                static_cast<std::uint8_t>(state[i] >> 24u);
            out[4u * i + 1u] =
                static_cast<std::uint8_t>(state[i] >> 16u);
            out[4u * i + 2u] =
                static_cast<std::uint8_t>(state[i] >> 8u);
            out[4u * i + 3u] =
                static_cast<std::uint8_t>(state[i]);
        }

        return out;
    }
};

inline int hex_value(char c) noexcept
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

} // namespace detail

inline sha256_digest sha256(
    const std::uint8_t *bytes,
    std::size_t size) noexcept
{
    detail::sha256_context context;

    if (bytes != nullptr && size != 0u)
        context.update(bytes, size);

    return context.finish();
}

inline bool matches_hex(
    const sha256_digest &digest,
    std::string_view text) noexcept
{
    if (text.size() != 64u)
        return false;

    for (std::size_t i = 0; i < digest.size(); ++i) {
        const int high = detail::hex_value(text[2u * i]);
        const int low = detail::hex_value(text[2u * i + 1u]);

        if (high < 0 || low < 0)
            return false;

        const auto value =
            static_cast<std::uint8_t>((high << 4) | low);

        if (digest[i] != value)
            return false;
    }

    return true;
}

} // namespace dsrrl::operators::legacy_plan::hashing
