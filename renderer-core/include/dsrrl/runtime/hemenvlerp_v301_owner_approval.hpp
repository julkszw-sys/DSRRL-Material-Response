#pragma once
// V301 owner-local diagnostic only. Generated from FULL V300 checked manifest;
// not a pixel-equivalence certificate, never bundled with retail DXBC bytes.
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::runtime::hemenvlerp_v301 {
struct owner_rx {
    std::uint32_t receiver_id;
    std::size_t size;
    const char* sha256;
};
inline constexpr std::array<owner_rx,0> approved{};
} // namespace
