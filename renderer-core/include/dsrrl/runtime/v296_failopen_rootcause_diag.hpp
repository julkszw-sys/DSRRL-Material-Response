#pragma once
// V350 V296 root-cause witness. Observe only; never changes draw authority.
// One log per exact texture/receiver/material/decision signature, max 64.
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <reshade.hpp>

namespace dsrrl::runtime::v296_diag {
inline std::array<std::atomic<std::uint64_t>,64> seen{};
inline std::atomic<std::uint64_t> suppressed{0};
inline std::atomic_bool capacity_warned{false};

template<class Material,class Decision,class Probe>
inline void record(const Material &m,const Decision &d,const Probe &p,
                   std::uint32_t rx,std::uint64_t frame) noexcept {
    // This is an actual V296 branch hit, NOT a claim that the DDS changed.
    // Source logical hash is that of the current stock t1, not a guessed alias.
    std::uint64_t key=0xcbf29ce484222325ULL;
    auto mix=[&key](std::uint64_t v) noexcept {
        key^=v; key*=0x100000001b3ULL;
    };
    mix(p.logical_hash); mix(rx); mix(m.route_index);
    mix(m.material_slot); mix(m.semantic_name_hash);
    mix(static_cast<unsigned>(d.reason)); mix(d.active ? 1u:0u);
    mix(m.owner_tuple_exact ? 1u:0u);
    mix(m.actual_material_exact ? 1u:0u);
    mix(p.companion_ready ? 1u:0u);
    if(key==0u)key=1u;
    thread_local std::uint64_t previous=0u;
    if(previous==key)return;
    previous=key;
    bool won=false;
    for(auto &entry:seen) {
        auto value=entry.load(std::memory_order_relaxed);
        if(value==key)return;
        if(value==0u && entry.compare_exchange_strong(
             value,key,std::memory_order_relaxed)) {won=true;break;}
    }
    if(!won) {
        suppressed.fetch_add(1u,std::memory_order_relaxed);
        if(!capacity_warned.exchange(true))
            reshade::log::message(reshade::log::level::warning,
                "[DSRRL V296 ROOTCAUSE] distinct_signature_capacity=64 exceeded; "
                "additional identities suppressed, draw behavior unchanged");
        return;
    }
    char out[860]{};
    std::snprintf(out,sizeof(out),
        "[DSRRL V296 ROOTCAUSE] first_frame=%llu rx=%u "
        "stock_t1_hash=%016llx stock_bound=%u name_resolved=%u "
        "oct10_manifest=%u sidecar_ready=%u "
        "material_valid=%u material_route=%u slot=%u slot_valid=%u "
        "flver_token=%016llx owner_tuple_exact=%u actual_mtd_exact=%u "
        "mtd_sem_hash=%016llx mtd_family_hash=%016llx "
        "decision_active=%u decision_reason=%u decision_rx=%u "
        "certified_ops=0x%X authorized_full_island=0 "
        "action=STOCK_DSR_MATERIAL cause=NO_CERTIFIED_ATOMIC_PTDE_CONSUMER "
        "texture_swap_observed=NO",
        static_cast<unsigned long long>(frame),rx,
        static_cast<unsigned long long>(p.logical_hash),
        p.stock_bound?1u:0u,p.snapshot_resolved?1u:0u,
        p.logical_hash_allowed?1u:0u,p.companion_ready?1u:0u,
        m.valid?1u:0u,m.route_index,m.material_slot,
        m.material_slot_valid?1u:0u,
        static_cast<unsigned long long>(m.flver_identity_hash),
        m.owner_tuple_exact?1u:0u,m.actual_material_exact?1u:0u,
        static_cast<unsigned long long>(m.semantic_name_hash),
        static_cast<unsigned long long>(m.material_family_hash),
        d.active?1u:0u,static_cast<unsigned>(d.reason),
        d.receiver_id,d.certified_operations);
    reshade::log::message(reshade::log::level::info,out);
}
} // namespace dsrrl::runtime::v296_diag
