#pragma once
#include <cstdint>
namespace dsrrl::runtime::dsr_only_lightbank_rows_v1 {
struct row { std::uint64_t signature; std::uint32_t id; std::uint16_t r,g,b,m; };
inline constexpr row rows[3] = {
 {0xc9b879a8c5e624d0ULL,64u,190u,235u,255u,70u}, // m14
 {0xe447e0e74af8d1d3ULL,64u,190u,235u,255u,70u}, // s14
 {0x1ecfd1e617c59071ULL,64u,255u,255u,255u,100u} // m18
};
constexpr const row *find(std::uint64_t signature,std::uint32_t id) noexcept {
 for (const auto &r:rows) if(r.signature==signature&&r.id==id)return &r;
 return nullptr;
}
} // namespace dsrrl::runtime::dsr_only_lightbank_rows_v1
