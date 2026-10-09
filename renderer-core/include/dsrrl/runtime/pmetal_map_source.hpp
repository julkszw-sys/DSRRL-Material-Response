#pragma once
// R43 P_Metal: bounded exact-material AND map-qualified LightBank source.
// Only original DSR bank signatures and 342-item binder ordinal ranges are used.
#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
namespace dsrrl::runtime::pmetal_map_source {
using material = operators::material_response::material_identity;
constexpr unsigned probe_area(unsigned ordinal) noexcept {
    constexpr unsigned end[9]={87,130,173,188,201,274,308,333,342};
    for(unsigned i=0;i<9;++i) if(ordinal<end[i])return i+10;
    return 0;
}
struct bank_info {std::uint64_t sig; unsigned area;};
inline constexpr std::array<bank_info,20> k_banks{{
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
constexpr unsigned bank_area(std::uint64_t signature) noexcept {
    // Exact original DSR-only layouts. These are source classes, not map
    // numbers: m99/s99 use a common 99-tag, default a 100-tag.
    if (signature == 0x34bdd6493ca1a91aULL ||
        signature == 0x91aa11098cee0fe2ULL) return 99u;
    if (signature == 0x96ece3bed03eed01ULL) return 100u;
    for(auto b:k_banks)if(b.sig==signature)return b.area;
    return 0;
}
inline bool source_in_area(const pmetal_envspec_source &src,unsigned area) noexcept {
    return ((area>=10 && area<=18) || area==99u || area==100u) && bank_area(src.bank_signature_a)==area &&
        (src.beta==0.0f || bank_area(src.bank_signature_b)==area);
}
inline bool same_material(const material &a,const material &b) noexcept {
    return a.valid==b.valid && a.owner_tuple_exact==b.owner_tuple_exact &&
        a.material_slot_valid==b.material_slot_valid &&
        a.actual_material_exact==b.actual_material_exact &&
        a.flver_sha256==b.flver_sha256 && a.raw_mtd_sha256==b.raw_mtd_sha256 &&
        a.material_slot==b.material_slot && a.route_index==b.route_index &&
        a.semantic_name_hash==b.semantic_name_hash &&
        a.material_family_hash==b.material_family_hash;
}
inline std::uint64_t fingerprint(const material &m,unsigned area) noexcept {
    std::uint64_t h=0xcbf29ce484222325ULL;
    auto byte=[&](std::uint8_t x){h=(h^x)*0x100000001b3ULL;};
    for(auto b:m.flver_sha256)byte(b);
    for(auto b:m.raw_mtd_sha256)byte(b);
    auto number=[&](std::uint64_t x){for(int i=0;i<8;++i)byte(static_cast<std::uint8_t>((x>>(i*8))&255u));};
    number(m.material_slot);number(m.route_index);number(m.semantic_name_hash);
    number(m.material_family_hash);number(area);
    return h;
}
struct entry {
    material owner{};
    pmetal_envspec_source source{};
    unsigned area=0;
    std::uint64_t generation=0;
    bool valid=false;
};
struct set {
    std::mutex mutex;
    std::array<entry,4> ways{};
    unsigned eviction=0;
};
inline std::array<set,256> groups{};
inline std::atomic<std::uint64_t> current_generation{1};
inline void invalidate() noexcept {
    current_generation.fetch_add(1u,std::memory_order_acq_rel);
}
inline bool publish(const material &owner,const pmetal_envspec_source &source,
                    unsigned selector_area) noexcept {
    if(!owner.valid || !owner.owner_tuple_exact || !owner.material_slot_valid ||
       !source_in_area(source,selector_area))
        return false;
    auto &s=groups[fingerprint(owner,selector_area)&255u];
    std::lock_guard<std::mutex> lock(s.mutex);
    const auto generation=current_generation.load(std::memory_order_acquire);
    for(auto &slot:s.ways) {
        if(slot.valid && slot.generation==generation && slot.area==selector_area &&
           same_material(slot.owner,owner)) {
            slot.source=source;
            return true;
        }
    }
    auto &slot=s.ways[s.eviction++%s.ways.size()];
    slot={owner,source,selector_area,generation,true};
    return true;
}
inline bool latest(const material &owner,unsigned area,pmetal_envspec_source &out) noexcept {
    out={};
    if(!owner.valid || !((area>=10 && area<=18)||area==99u||area==100u))return false;
    auto &s=groups[fingerprint(owner,area)&255u];
    std::lock_guard<std::mutex> lock(s.mutex);
    const auto generation=current_generation.load(std::memory_order_acquire);
    for(const auto &slot:s.ways) {
        if(slot.valid && slot.generation==generation && slot.area==area &&
           same_material(slot.owner,owner) && source_in_area(slot.source,area)) {
            out=slot.source;
            return true;
        }
    }
    return false;
}
static_assert(probe_area(70)==10 && probe_area(337)==18);
static_assert(probe_area(341)==18 && probe_area(342)==0);
static_assert(bank_area(0x1ecfd1e617c59071ULL)==18);
static_assert(bank_area(0x34bdd6493ca1a91aULL)==99);
static_assert(bank_area(0x91aa11098cee0fe2ULL)==99);
static_assert(bank_area(0x96ece3bed03eed01ULL)==100);
static_assert(bank_area(0x4c594553d201d80cULL)==10);
} // namespace dsrrl::runtime::pmetal_map_source
