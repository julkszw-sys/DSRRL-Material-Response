#pragma once
// V411: actual RX34 indexed_issued receiver observation, never gates pixels.
#include "dsrrl/runtime/v411_owner_sequence_journal.hpp"
#include "dsrrl/runtime/v413_live_owner_probe.hpp"
#include "dsrrl/runtime/v409_rx34_transition_tracker.hpp"
#include "dsrrl/runtime/pmetal_producer_state.hpp"
#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"
#include "dsrrl/runtime/runtime_hot_telemetry.hpp"
#include <Windows.h>
#include <reshade.hpp>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dsrrl::runtime::v411 {
inline void indexed_rx34(
    const operators::material_response::material_identity &m,
    const operators::material_response::decision &d,
    const prepared_pmetal_envspec_draw &p) noexcept
{
    if(d.receiver_id!=34u || !p.ready ||
       !p.stable_envdiffuse_trace_valid ||
       !telemetry::effect_enabled()) return;
    // This is the same exact material key as V409, but independent
    // bookkeeping makes the owner/wrapper journal test auditable.
    static v409::source_transition_tracker transitions{};
    v409::identity key{};
    key.owner=m.model_owner_instance;
    key.shader=reinterpret_cast<std::uintptr_t>(p.shader);
    key.route=m.route_index;
    key.material_slot=m.material_slot;
    key.semantic=m.semantic_name_hash;
    key.receiver=d.receiver_id;
    key.flver=m.flver_sha256;key.mtd=m.raw_mtd_sha256;
    v409::source_snapshot source{};
    source.kind=p.source_kind;
    source.independent=p.stable_envdiffuse_independent;
    source.bank=p.stable_envdiffuse_bank;
    source.row=p.stable_envdiffuse_row;
    source.probe=p.stable_envdiffuse_probe;
    source.serial=p.stable_envdiffuse_source_serial;
    source.native_sequence=p.stable_envdiffuse_native_producer_sequence;
    for(std::size_t i=0;i<3u;++i)
        std::memcpy(&source.rgb_bits[i],
            &p.stable_envdiffuse_c86[i],sizeof(float));
    const auto change=transitions.observe(key,source);
    const bool first=change.kind==v409::observation_kind::first &&
        change.sequence<=64u;
    const bool switched=change.kind==v409::observation_kind::transition &&
        (change.sequence<=512u || (change.sequence%4096u)==0u);
    const bool overflow=change.kind==v409::observation_kind::capacity_miss &&
        (change.sequence<=16u || (change.sequence%4096u)==0u);
    if(!first && !switched && !overflow) return;
    // Source2 has no actor-qualified native sequence. Keep its routing
    // independent and protected; report it without a positive journal join.
    const bool source1=p.source_kind==1u;
    const auto live=source1 ? v413::inspect_owner(m.model_owner_instance) :
        v413::native_context_tuple{};
    lookup_result joined{};
    if(source1) {
        if(!live.readable) joined.state=status::context_unreadable;
        else if(!live.manager_agrees) joined.state=status::native_manager_disagreement;
        else {
            joined=observations().lookup(m.model_owner_instance,
                p.stable_envdiffuse_native_producer_sequence,
                p.stable_envdiffuse_source_serial,live.wrapper,live.manager);
            if(joined.state==status::matched_snapshot &&
               joined.prior.vtable_readable &&
               joined.prior.context_vtable!=live.vtable)
                joined.state=status::context_tuple_changed;
        }
    }
    pmetal_envspec_source currently{};
    const bool cache_available=source1 &&
        pmetal_producer_state_latest(
            m,p.stable_envdiffuse_source_serial,currently);
    const bool cache_seq_matches=cache_available &&
        p.stable_envdiffuse_native_producer_sequence!=0u &&
        currently.native_producer_sequence==
            p.stable_envdiffuse_native_producer_sequence;
    // Cached snapshot may have been superseded after draw preparation.
    // No proof of native actor lifetime or GPU command order is claimed.
    constexpr char hex[]="0123456789abcdef";
    char flver[65]{},mtd[65]{};
    for(std::size_t i=0;i<32u;++i) {
        flver[i*2u]=hex[key.flver[i]>>4u];
        flver[i*2u+1u]=hex[key.flver[i]&15u];
        mtd[i*2u]=hex[key.mtd[i]>>4u];
        mtd[i*2u+1u]=hex[key.mtd[i]&15u];
    }
    char line[1650]{};
    std::snprintf(line,sizeof(line),
        "[DSRRL V411 RX34 OWNER SEQ DRAW] "
        "event=%s n=%llu owner=%p route=%u slot=%u "
        "shader=%p flver_sha=%s mtd_sha=%s "
        "owner_tuple_exact=%u kind=%u row=%u bank=%016llx "
        "serial=%llu native_producer_seq=%llu "
        "stamp_state=%u stamp_version=%llu stamp_wrapper=%p "
        "stamp_manager=%p stamp_source_a=%p stamp_source_b=%p "
        "stamp_selector_a=%d stamp_selector_b=%d "
        "stamp_beta_bits=%08x stamp_row_a=%u stamp_row_b=%u "
        "stamp_vtable=%p stamp_vtable_readable=%u "
        "stamp_context_manager=%p manager_field_readable=%u "
        "manager_matches=%u pointer_reassignment_seen=%u "
        "historical_snapshot=%u history_evicted=%u "
        "stamp_candidates=%u history_overwrites=%llu "
        "cache_readback=%u cache_seq_matches=%u "
        "previous_kind=%u previous_row=%u previous_seq=%llu "
        "tick_ms=%llu tid=%lu issued=1 "
        "native_actor_call=OPEN command_order=OPEN pixel=OPEN",
        switched?"transition":first?"first":"capacity_miss",
        static_cast<unsigned long long>(change.sequence),
        reinterpret_cast<void*>(m.model_owner_instance),
        m.route_index,m.material_slot,
        reinterpret_cast<void*>(p.shader),flver,mtd,
        m.owner_tuple_exact?1u:0u,
        static_cast<unsigned>(p.source_kind),
        p.stable_envdiffuse_row,
        static_cast<unsigned long long>(p.stable_envdiffuse_bank),
        static_cast<unsigned long long>(
            p.stable_envdiffuse_source_serial),
        static_cast<unsigned long long>(
            p.stable_envdiffuse_native_producer_sequence),
        source1 ? static_cast<unsigned>(joined.state) : 255u,
        static_cast<unsigned long long>(joined.observed_version),
        reinterpret_cast<void*>(joined.prior.wrapper),
        reinterpret_cast<void*>(joined.prior.manager),
        reinterpret_cast<void*>(joined.prior.source_a),
        reinterpret_cast<void*>(joined.prior.source_b),
        static_cast<int>(joined.prior.selector_a),
        static_cast<int>(joined.prior.selector_b),
        joined.prior.beta_bits,
        joined.prior.row_a,joined.prior.row_b,
        reinterpret_cast<void*>(joined.prior.context_vtable),
        joined.prior.vtable_readable?1u:0u,
        reinterpret_cast<void*>(joined.prior.context_manager),
        joined.prior.manager_field_readable?1u:0u,
        joined.prior.context_manager_matches_wrapper?1u:0u,
        joined.had_pointer_reassignment?1u:0u,
        joined.superseded_by_newer?1u:0u,
        joined.history_has_evictions?1u:0u,
        static_cast<unsigned>(joined.candidates),
        static_cast<unsigned long long>(
            observations().history_overwrites()),
        cache_available?1u:0u,cache_seq_matches?1u:0u,
        static_cast<unsigned>(change.previous.kind),
        change.previous.row,
        static_cast<unsigned long long>(
            change.previous.native_sequence),
        static_cast<unsigned long long>(GetTickCount64()),
        static_cast<unsigned long>(GetCurrentThreadId()));
    reshade::log::message(reshade::log::level::warning,line);
}
}
