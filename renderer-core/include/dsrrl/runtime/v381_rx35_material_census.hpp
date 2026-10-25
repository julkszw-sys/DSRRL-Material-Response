#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <reshade.hpp>
namespace dsrrl::runtime {
inline void v381_rx35_census(
    unsigned phase,const char *stage,bool indexed,unsigned route_mask,
    const operators::material_response::material_identity &m,
    const operators::material_response::decision &d,
    unsigned island_flags,unsigned island_count,unsigned source_kind,
    unsigned resource_count,bool issued) noexcept {
    if(d.receiver_id!=35u || !m.valid || !m.owner_tuple_exact ||
       !m.material_slot_valid || phase>7u) return;
    std::uint64_t key=m.flver_identity_hash^m.semantic_name_hash;
    key^=static_cast<std::uint64_t>(m.route_index)*0x9e3779b97f4a7c15ull;
    key^=static_cast<std::uint64_t>(m.material_slot)<<16u;
    key^=static_cast<std::uint64_t>(phase)<<5u;
    key^=indexed?0xd1687e391ccad873ull:0xe27aac912783be45ull;
    key|=1ull;
    static std::array<std::atomic<std::uint64_t>,512u> seen{};
    const auto slot=key%seen.size();
    bool claimed=false;
    for(unsigned i=0;i<12u;++i) {
        auto &cell=seen[(slot+i)%seen.size()];
        if(cell.load(std::memory_order_relaxed)==key)return;
        std::uint64_t empty=0u;
        if(cell.compare_exchange_strong(empty,key,std::memory_order_relaxed)) {
            claimed=true;break;
        }
    }
    if(!claimed)return;
    char line[500]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V381 RX35 ALL MATERIAL CUT] stage=%s indexed=%u "
        "route=%u slot=%u flver=%016llx owner=1 mtd=%u "
        "active=%u ops=%08x mask=%02x flags=%02x islands=%u "
        "source_kind=%u resources=%u issued=%u pixel_pass=0",
        stage,indexed?1u:0u,static_cast<unsigned>(m.route_index),
        static_cast<unsigned>(m.material_slot),
        static_cast<unsigned long long>(m.flver_identity_hash),
        m.actual_material_exact?1u:0u,d.active?1u:0u,
        static_cast<unsigned>(d.certified_operations),route_mask,
        island_flags,island_count,source_kind,resource_count,issued?1u:0u);
    reshade::log::message(reshade::log::level::warning,line);
}
}