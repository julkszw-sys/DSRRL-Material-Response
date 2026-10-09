#pragma once
#include <array>
#include <cstdint>
namespace dsrrl::runtime::pmetal_map_provenance {
struct range { unsigned start, end; unsigned map; };
inline constexpr std::array<range,9> spans = {{{0,87,10},{87,130,11},{130,173,12},{173,188,13},{188,201,14},{201,274,15},{274,308,16},{308,333,17},{333,342,18}}};
constexpr unsigned probe_area(unsigned id) noexcept {for (auto r:spans) if(id>=r.start&&id<r.end)return r.map;return 0;}
// Fixed DSR V13 signatures from pmetal_env_source_layout_v1.tsv.
// Each signature is the exact bank identity, not a guessed active map.
struct source_area { std::uint64_t signature; unsigned map; };
inline constexpr std::array<source_area,20> banks = {{
{0x4c594553d201d80cULL,10},{0xa710f288bd3aca82ULL,10},
{0x872153a097540e78ULL,11},{0x8a953042ffb0d411ULL,11},
{0xcf6930ac9b0026baULL,12},{0xfc11b271fc756886ULL,12},
{0xcdfb3e57c1e8b541ULL,13},{0x828061f1e486e868ULL,13},
{0xc9b879a8c5e624d0ULL,14},{0xe447e0e74af8d1d3ULL,14},
{0xcef8834cc83a00d0ULL,15},{0x6da2e3578ff67bafULL,15},
{0x7434232b8893cbb5ULL,15},{0x9b453af04c8f1f6bULL,15},
{0x71bd152ef1e5e3dfULL,16},{0x646466fd09374c5bULL,16},
{0x920175710a0728ecULL,17},{0xe28e459ed303b5b0ULL,17},
{0x1ecfd1e617c59071ULL,18},{0x4bf694db53edacc5ULL,18}
}};
constexpr unsigned source_map(std::uint64_t signature) noexcept {
    for (auto x:banks) if(x.signature==signature)return x.map;
    return 0;
}
// A must match bound probe A; when B is consumed, both endpoints must
// belong to that same map. Unknowns fail open, never remap bank rows.
constexpr bool match(std::uint64_t bank_a,std::uint64_t bank_b,
                     unsigned probe_a,unsigned probe_b,bool b_active) noexcept {
    const unsigned m=source_map(bank_a);
    if(!m||probe_area(probe_a)!=m)return false;
    if(b_active && (source_map(bank_b)!=m||probe_area(probe_b)!=m))return false;
    return true;
}
static_assert(match(0x4c594553d201d80cULL,0,70,0,false));
static_assert(match(0x1ecfd1e617c59071ULL,0,337,0,false));
static_assert(!match(0x4c594553d201d80cULL,0,337,0,false));
static_assert(!match(0x1ecfd1e617c59071ULL,0,70,0,false));
static_assert(!match(0x1ecfd1e617c59071ULL,0,342,0,false));

}
