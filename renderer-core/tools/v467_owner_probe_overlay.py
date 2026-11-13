#!/usr/bin/env python3
"""V467: read-only liveness census of V465 ChrIns observer and same-draw typed
WorldChrManImp+0x68 -> PlayerIns -> ChrModel handle vs issued RX34 handle.

Run after V466. Retains one addon and its existing hooks; no owner inference
from FLVER, resource sharing, slot index or non-player handle mismatch.
"""
from pathlib import Path
ROOT=Path("renderer-core")
HEADER=ROOT/"include/dsrrl/runtime/v438_one_shot_trace.hpp"
HOOK=ROOT/"src/runtime/flver_engine_hooks.cpp"
ISSUE=ROOT/"integrated/integrated_addon.cpp"

def change(file,old,new,label):
    s=file.read_text(encoding="utf-8")
    n=s.count(old)
    assert n==1,(str(file),label,n)
    file.write_text(s.replace(old,new,1),encoding="utf-8")

# Observe actual entry calls independently from hook installation.
change(HOOK,
'''std::uintptr_t __fastcall v465_chrins_entry(void *owner,void *out) noexcept {
    //''',
'''std::uintptr_t __fastcall v465_chrins_entry(void *owner,void *out) noexcept {
    dsrrl::runtime::v465::note_chrins_entry(owner!=nullptr);
    //''',"source entry liveness")
change(HOOK,
'''        // V465 Ally: log-only aggregate; no event ring/CSV.
        dsrrl::runtime::v465::observe_owner(''',
'''        // V467: separately census which exact class actually reached this hook.
        dsrrl::runtime::v465::note_chrins_type(type);
        // V465 Ally: log-only aggregate; no event ring/CSV.
        dsrrl::runtime::v465::observe_owner(''',"typed native hook census")

change(HEADER,
''' std::uint64_t owner_hits=0u,pre_owner_hits=0u,live_owner_hits=0u,
               draw_hits=0u,row30=0u,row31=0u;
 bool ownership_conflict=false,entity_conflict=false;''',
''' std::uint64_t owner_hits=0u,pre_owner_hits=0u,live_owner_hits=0u,
               draw_hits=0u,row30=0u,row31=0u;
 std::uint64_t world_player_matches=0u;
 std::uintptr_t world_player_actor=0u,world_player_model=0u;
 bool ownership_conflict=false,entity_conflict=false,world_actor_conflict=false;''',"per-handle independent world witness")

change(HEADER,
''' std::atomic<std::uint64_t> invalid_owners{0u},invalid_draws{0u};
};''',
''' std::atomic<std::uint64_t> invalid_owners{0u},invalid_draws{0u};
 std::atomic<std::uint64_t> native_entries{0u},native_nonnull{0u};
 std::atomic<std::uint64_t> native_player{0u},native_enemy{0u},native_base_chr{0u},native_unknown{0u};
 std::atomic<std::uint64_t> world_queries{0u},world_skipped{0u},world_exact{0u};
 std::atomic<std::uint64_t> world_player_exact{0u},world_model_exact{0u};
 std::atomic<std::uint64_t> world_handle_read{0u},world_full_match{0u},world_full_mismatch{0u};
};
''',"liveness and typed world counters")

change(HEADER,
'''inline void note_rx34() noexcept {
 samples().scene_rx34.fetch_add(1u,std::memory_order_relaxed);
}
inline actor_record *find_locked''',
'''inline void note_rx34() noexcept {
 samples().scene_rx34.fetch_add(1u,std::memory_order_relaxed);
}
inline void note_chrins_entry(bool nonnull) noexcept {
 auto &s=samples();
 s.native_entries.fetch_add(1u,std::memory_order_relaxed);
 if(nonnull)s.native_nonnull.fetch_add(1u,std::memory_order_relaxed);
}
inline void note_chrins_type(std::uint32_t type) noexcept {
 auto &s=samples();
 if(type==1u)s.native_player.fetch_add(1u,std::memory_order_relaxed);
 else if(type==2u)s.native_enemy.fetch_add(1u,std::memory_order_relaxed);
 else if(type==3u)s.native_base_chr.fetch_add(1u,std::memory_order_relaxed);
 else s.native_unknown.fetch_add(1u,std::memory_order_relaxed);
}
inline void note_world_skipped() noexcept {
 samples().world_skipped.fetch_add(1u,std::memory_order_relaxed);
}
inline actor_record *find_locked''',"native liveness functions")

change(HEADER,
'''inline void finish() noexcept {
 auto &st=samples();''',
'''// Independent same-draw typed PlayerIns association. DOES NOT populate
// the V465 hook-derived owner fields and DOES NOT promote CPU_JOIN.
inline void observe_world_player(
    std::uintptr_t entity,std::uint32_t issued_h,
    std::uintptr_t player,std::uintptr_t model,std::uint32_t player_h,
    std::uint32_t flags) noexcept {
 if(!dsrrl::runtime::v438::state().active.load(std::memory_order_acquire))return;
 auto &st=samples();
 st.world_queries.fetch_add(1u,std::memory_order_relaxed);
 if(flags&1u)st.world_exact.fetch_add(1u,std::memory_order_relaxed);
 if(flags&2u)st.world_player_exact.fetch_add(1u,std::memory_order_relaxed);
 if(flags&4u)st.world_model_exact.fetch_add(1u,std::memory_order_relaxed);
 if(flags&8u)st.world_handle_read.fetch_add(1u,std::memory_order_relaxed);
 if((flags&0x0fu)!=0x0fu || !player || !model || !entity ||
    !(issued_h&0x80000000u) || !(player_h&0x80000000u))return;
 if(!(flags&16u) || player_h!=issued_h){
   st.world_full_mismatch.fetch_add(1u,std::memory_order_relaxed);return;
 }
 st.world_full_match.fetch_add(1u,std::memory_order_relaxed);
 if(!st.lock.try_lock()){
   st.lock_drops.fetch_add(1u,std::memory_order_relaxed);return;
 }
 auto *entry=find_locked(st,issued_h);
 if(entry){
   if((entry->world_player_actor && entry->world_player_actor!=player) ||
      (entry->world_player_model && entry->world_player_model!=model) ||
      (entry->entity && entry->entity!=entity))
      entry->world_actor_conflict=true;
   if(!entry->world_player_actor)entry->world_player_actor=player;
   if(!entry->world_player_model)entry->world_player_model=model;
   ++entry->world_player_matches;
 }
 st.lock.unlock();
}
inline void finish() noexcept {
 auto &st=samples();''',"independent issue-side typed world census")

change(HEADER,
'''    reshade::log::message(reshade::log::level::warning,buf);
 }
 char final[512]{};''',
'''    reshade::log::message(reshade::log::level::warning,buf);
    if(v.world_player_matches){
      char wb[384]{};
      std::snprintf(wb,sizeof(wb),
       "[DSRRL V467] WORLD_PLAYER_HANDLE status=%s handle=%08x actor=%llx model=%llx issued_entity=%llx same_draw_matches=%llu row30=%llu row31=%llu owner_hook_hits=%llu actor_conflict=%u",
       v.world_actor_conflict?"UNKNOWN_CONFLICT":"TYPED_PLAYER_CANDIDATE",
       v.full_handle,
       static_cast<unsigned long long>(v.world_player_actor),
       static_cast<unsigned long long>(v.world_player_model),
       static_cast<unsigned long long>(v.entity),
       static_cast<unsigned long long>(v.world_player_matches),
       static_cast<unsigned long long>(v.row30),
       static_cast<unsigned long long>(v.row31),
       static_cast<unsigned long long>(v.owner_hits),
       v.world_actor_conflict?1u:0u);
      reshade::log::message(reshade::log::level::warning,wb);
    }
 }
 char final[512]{};''',"per-handle typed player candidate output")

change(HEADER,
''' reshade::log::message(reshade::log::level::warning,final);
}
}''',
''' reshade::log::message(reshade::log::level::warning,final);
 char diag[768]{};
 std::snprintf(diag,sizeof(diag),
   "[DSRRL V467] OWNER_SOURCE_DIAG native_entries=%llu native_nonnull=%llu native_player=%llu native_enemy=%llu native_base_chr=%llu native_unknown=%llu world_queries=%llu world_skipped=%llu world_exact=%llu player_exact=%llu model_exact=%llu player_handle_read=%llu world_full_match=%llu world_full_mismatch=%llu source_hook_call=OPEN_UNTIL_ENTRY_SEEN actor_pixel=OPEN PTDE_pixel=OPEN",
   static_cast<unsigned long long>(st.native_entries.load()),
   static_cast<unsigned long long>(st.native_nonnull.load()),
   static_cast<unsigned long long>(st.native_player.load()),
   static_cast<unsigned long long>(st.native_enemy.load()),
   static_cast<unsigned long long>(st.native_base_chr.load()),
   static_cast<unsigned long long>(st.native_unknown.load()),
   static_cast<unsigned long long>(st.world_queries.load()),
   static_cast<unsigned long long>(st.world_skipped.load()),
   static_cast<unsigned long long>(st.world_exact.load()),
   static_cast<unsigned long long>(st.world_player_exact.load()),
   static_cast<unsigned long long>(st.world_model_exact.load()),
   static_cast<unsigned long long>(st.world_handle_read.load()),
   static_cast<unsigned long long>(st.world_full_match.load()),
   static_cast<unsigned long long>(st.world_full_mismatch.load()));
 reshade::log::message(reshade::log::level::warning,diag);
}
}''',"source-stage counters")

# After existing certified RX34 issued handle capture, resolve a DIFFERENT
# type-confirmed WorldChrManImp/PlayerIns/ChrModel cut in that same draw.
change(ISSUE,
'''        dsrrl::runtime::v465::observe_draw(
            entity459,handle465,rx447.row_a,status465);''',
'''        dsrrl::runtime::v465::observe_draw(
            entity459,handle465,rx447.row_a,status465);
        // V467: same issued draw, Source1 only. Unknown -> stock behavior.
        // WorldChrManImp+0x68 is CANDIDATE: exact vtables at each hop.
        if(static_cast<unsigned>(prepared.source_kind)==1u &&
           (status465&7u)==7u) {
            std::uintptr_t world467=0u,world_vt467=0u;
            std::uintptr_t actor467=0u,actor_vt467=0u;
            std::uintptr_t model467=0u,model_vt467=0u;
            std::uint32_t player_h467=0u,flags467=0u;
            if(base459 &&
               dsrrl::runtime::v443::read(base459+0x1C77E50u,world467) &&
               world467 &&
               dsrrl::runtime::v443::read(world467,world_vt467) &&
               world_vt467==base459+0x1328210u) {
                flags467|=1u; // exact native WorldChrManImp
                if(dsrrl::runtime::v443::read(world467+0x68u,actor467) &&
                   actor467 &&
                   dsrrl::runtime::v443::read(actor467,actor_vt467) &&
                   actor_vt467==base459+0x13251F0u) {
                    flags467|=2u; // exact native PlayerIns
                    if(dsrrl::runtime::v443::read(actor467+0x60u,model467) &&
                       model467 &&
                       dsrrl::runtime::v443::read(model467,model_vt467) &&
                       model_vt467==base459+0x12EAF30u) {
                        flags467|=4u; // exact native ChrModel
                        if(dsrrl::runtime::v443::read(model467+0x10u,player_h467)) {
                            flags467|=8u;
                            if(player_h467==handle465)flags467|=16u;
                        }
                    }
                }
            }
            dsrrl::runtime::v465::observe_world_player(
                entity459,handle465,actor467,model467,player_h467,flags467);
        }else{
            dsrrl::runtime::v465::note_world_skipped();
        }''',"issued RX34 typed world player probe")

print("V467 overlay PASS: actual source method entry census plus same-draw typed WorldChrManImp PlayerIns full-handle witness; one addon; fail-open")
