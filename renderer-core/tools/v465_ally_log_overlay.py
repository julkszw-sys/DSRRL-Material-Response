#!/usr/bin/env python3
"""V465 Ally log-only auto capture, NO keyboard, NO CSV, NO unbounded frame logging.
Run after v465_actor_owner_overlay.py. One integrated addon, no renderer patch.
"""
from pathlib import Path
R=Path("renderer-core")
t=R/"include/dsrrl/runtime/v438_one_shot_trace.hpp"
f=R/"src/runtime/flver_engine_hooks.cpp"
i=R/"integrated/integrated_addon.cpp"
s=R/"src/runtime/pmetal_env_source_runtime.cpp"
def one(path,old,new):
    v=path.read_text(encoding="utf-8")
    assert v.count(old)==1,(str(path),v.count(old),old[:100])
    path.write_text(v.replace(old,new,1),encoding="utf-8")
one(t,"#include <atomic>","#include <atomic>\n#include <array>\n#include <mutex>")
one(t,"namespace dsrrl::runtime::v438 {",
"""namespace dsrrl::runtime::v465 {
std::uint64_t rx34_count() noexcept;
void finish() noexcept;
}
namespace dsrrl::runtime::v438 {""")
h=t.read_text(encoding="utf-8")
a=h.index("inline void emit(kind t,")
b=h.index("inline void drain(",a)
h=h[:a]+'''// V465 log-only: old ring/CSV emission is intentionally disabled.
inline void emit(kind, std::initializer_list<u64> = {}) noexcept {}
'''+h[b:]
a=h.index("inline void on_present() noexcept {")
# The old on_present is the last method in namespace v438; replace its body
# and close the namespace without modifying existing fixed-size event ABI.
assert h.rstrip().endswith("}\n}"),"V465 v438 header ending drift"
h=h[:a]+'''inline void on_present() noexcept {
 auto &s=state();
 const auto frame=s.frames.fetch_add(1u,std::memory_order_relaxed)+1u;
 if(!s.start_attempted.load(std::memory_order_acquire) &&
    frame>=30u && dsrrl::runtime::v465::rx34_count()>=16u) {
   // On scene detection, not app launch/menu. No F9, F10 or other key.
   s.start_attempted.store(true,std::memory_order_release);
   s.started_at_ms.store(GetTickCount64(),std::memory_order_release);
   s.active.store(true,std::memory_order_release);
   reshade::log::message(reshade::log::level::warning,
       "[DSRRL V465] ALLY_AUTO_CAPTURE_START RX34>=16 duration=25s output=ReShade.log no_CSV=1 no_keyboard=1");
 }
 if(!s.active.load(std::memory_order_acquire))return;
 if(GetTickCount64()-s.started_at_ms.load(std::memory_order_acquire)<25000u)return;
 s.active.store(false,std::memory_order_release);
 dsrrl::runtime::v465::finish();
 s.finished.store(true,std::memory_order_release);
 reshade::log::message(reshade::log::level::warning,
       "[DSRRL V465] ALLY_AUTO_CAPTURE_END output=ReShade.log no_CSV=1 no_keyboard=1");
}
}

namespace dsrrl::runtime::v465 {
struct actor_record {
 std::uint32_t full_handle=0u;
 std::uint32_t owner_type=0u,owner_flags=0u;
 std::uintptr_t owner=0u,model=0u,entity=0u;
 std::uint64_t owner_hits=0u,draw_hits=0u,row30=0u,row31=0u;
 bool ownership_conflict=false,entity_conflict=false;
};
struct sample_state {
 std::mutex lock{};
 std::array<actor_record,256> items{};
 std::atomic<std::uint64_t> scene_rx34{0u},lock_drops{0u},overflow{0u};
 std::atomic<std::uint64_t> invalid_owners{0u},invalid_draws{0u};
};
inline sample_state &samples() noexcept {
 static sample_state st{};
 return st;
}
inline std::uint64_t rx34_count() noexcept {
 return samples().scene_rx34.load(std::memory_order_relaxed);
}
inline void note_rx34() noexcept {
 samples().scene_rx34.fetch_add(1u,std::memory_order_relaxed);
}
inline actor_record *find_locked(sample_state &st,std::uint32_t h) noexcept {
 actor_record *empty=nullptr;
 for(auto &entry:st.items){
    if(entry.full_handle==h)return &entry;
    if(!empty && entry.full_handle==0u)empty=&entry;
 }
 if(empty)empty->full_handle=h;
 else st.overflow.fetch_add(1u,std::memory_order_relaxed);
 return empty;
}
inline void observe_owner(
    std::uintptr_t owner,std::uintptr_t model,std::uint32_t h,
    std::uint32_t type,std::uint32_t flags) noexcept {
 if(!dsrrl::runtime::v438::state().active.load(std::memory_order_acquire))return;
 if(!(flags&0x0fu) || (flags&0x0fu)!=0x0fu ||
    !owner || !model || !type || !(h&0x80000000u)) {
    samples().invalid_owners.fetch_add(1u,std::memory_order_relaxed);
    return;
 }
 auto &st=samples();
 if(!st.lock.try_lock()){
    st.lock_drops.fetch_add(1u,std::memory_order_relaxed);return;
 }
 auto *entry=find_locked(st,h);
 if(entry){
   if(entry->owner &&
      (entry->owner!=owner || entry->model!=model ||
       entry->owner_type!=type))
       entry->ownership_conflict=true;
   if(!entry->owner){
       entry->owner=owner;entry->model=model;entry->owner_type=type;
   }
   entry->owner_flags|=flags;
   ++entry->owner_hits;
 }
 st.lock.unlock();
}
inline void observe_draw(
    std::uintptr_t entity,std::uint32_t h,
    std::uint32_t row,std::uint32_t status) noexcept {
 if(!dsrrl::runtime::v438::state().active.load(std::memory_order_acquire))return;
 if(!entity || (status&0x07u)!=0x07u ||
    !(h&0x80000000u) || (row!=30u && row!=31u)){
    samples().invalid_draws.fetch_add(1u,std::memory_order_relaxed);return;
 }
 auto &st=samples();
 if(!st.lock.try_lock()){
    st.lock_drops.fetch_add(1u,std::memory_order_relaxed);return;
 }
 auto *entry=find_locked(st,h);
 if(entry){
    if(entry->entity && entry->entity!=entity)
        entry->entity_conflict=true;
    if(!entry->entity)entry->entity=entity;
    ++entry->draw_hits;
    if(row==30u)++entry->row30;else ++entry->row31;
 }
 st.lock.unlock();
}
inline void finish() noexcept {
 auto &st=samples();
 std::lock_guard<std::mutex> lock(st.lock);
 unsigned entries=0u,joined=0u,unknown=0u,conflicts=0u;
 std::uint64_t owners=0u,draws=0u;
 for(const auto &v:st.items){
    if(!v.full_handle)continue;
    ++entries;owners+=v.owner_hits;draws+=v.draw_hits;
    if(v.ownership_conflict||v.entity_conflict)++conflicts;
    const bool joined_ok=v.owner && v.entity &&
      (v.owner_type==1u || v.owner_type==2u) &&
      !v.ownership_conflict && !v.entity_conflict &&
      ((v.row30>0u) != (v.row31>0u));
    if(joined_ok)++joined;else ++unknown;
    char buf[512]{};
    // Once per unique native handle, not once per draw.
    // Treat EnemyIns as class identity, never as proof of hostile NPC.
    std::snprintf(buf,sizeof(buf),
      "[DSRRL V465] ACTOR_HANDLE status=%s handle=%08x class=%s owner=%llx model=%llx entity=%llx row30=%llu row31=%llu owner_hits=%llu draw_hits=%llu conflicting_owner=%u conflicting_receiver=%u",
      joined_ok?"CPU_JOIN":"UNKNOWN",v.full_handle,
      v.owner_type==1u?"PlayerIns":v.owner_type==2u?"EnemyIns":
      v.owner_type==3u?"ChrIns":"UNRESOLVED",
      static_cast<unsigned long long>(v.owner),
      static_cast<unsigned long long>(v.model),
      static_cast<unsigned long long>(v.entity),
      static_cast<unsigned long long>(v.row30),
      static_cast<unsigned long long>(v.row31),
      static_cast<unsigned long long>(v.owner_hits),
      static_cast<unsigned long long>(v.draw_hits),
      v.ownership_conflict?1u:0u,v.entity_conflict?1u:0u);
    reshade::log::message(reshade::log::level::warning,buf);
 }
 char final[512]{};
 std::snprintf(final,sizeof(final),
   "[DSRRL V465] SUMMARY handles=%u joined=%u unknown=%u conflicts=%u owner_samples=%llu rx34_draw_samples=%llu rx34_trigger_hits=%llu invalid_owner=%llu invalid_draw=%llu lock_drops=%llu overflow=%llu actor_pixel=OPEN PTDE_pixel=OPEN",
   entries,joined,unknown,conflicts,
   static_cast<unsigned long long>(owners),
   static_cast<unsigned long long>(draws),
   static_cast<unsigned long long>(rx34_count()),
   static_cast<unsigned long long>(st.invalid_owners.load()),
   static_cast<unsigned long long>(st.invalid_draws.load()),
   static_cast<unsigned long long>(st.lock_drops.load()),
   static_cast<unsigned long long>(st.overflow.load()));
 reshade::log::message(reshade::log::level::warning,final);
}
}
'''
t.write_text(h,encoding="utf-8")

# Replace unconditional native hot event with bounded per-handle in-memory
# owner collection (even when the method is hit hundreds of times/second).
f_code=f.read_text(encoding="utf-8")
a=f_code.index("        // Same F9 8s session and ring;")
b=f_code.index("    }\n    return g_v465_chrins_original?",a)
f_code=f_code[:a]+'''        // V465 Ally: log-only aggregate; no event ring/CSV.
        dsrrl::runtime::v465::observe_owner(
            addr,model,handle,type,flags);
'''+f_code[b:]
f.write_text(f_code,encoding="utf-8")

p=i.read_text(encoding="utf-8")
needle='''        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V465_RX34_HANDLE,{
            rx447.native_sequence,issued448.w.entity,handle465,
            rx447.row_a,static_cast<unsigned>(prepared.source_kind),status465,
            issued448.w.dp,issued448.w.engine_model,
            material.material_slot,material.route_index,vt465,
            static_cast<unsigned>(issued448.status),
            rx447.context,issued448.w.packet,
            issued448.w.frame_rbp});'''
assert p.count(needle)==1
p=p.replace(needle,'''        dsrrl::runtime::v465::observe_draw(
            entity459,handle465,rx447.row_a,status465);''',1)
# The trigger is independent of the now-active capture window, so the
# game can automatically recognise a scene before beginning the test.
needle='''        if(dsrrl::runtime::v438::state().active.load(std::memory_order_acquire) &&
           decision.receiver_id==34u && material.route_index==345u &&
           (prepared.stable_envdiffuse_row==30u ||
            prepared.stable_envdiffuse_row==31u)) {'''
assert p.count(needle)==1,(p.count(needle), "native issue eligibility drift")
p=p.replace(needle,'''        if(decision.receiver_id==34u && material.route_index==345u &&
           (prepared.stable_envdiffuse_row==30u ||
            prepared.stable_envdiffuse_row==31u))
            dsrrl::runtime::v465::note_rx34();
'''+needle,1)
i.write_text(p,encoding="utf-8")
v=s.read_text(encoding="utf-8")
old='''"[DSRRL V465] CHRINS_OWNER_FULL_HANDLE_RX34 F9_8s read_only=1 exact_type=1 unknown_failopen=1 pixel=OPEN");'''
assert v.count(old)==1
v=v.replace(old,''' "[DSRRL V465] ALLY_LOG_ONLY_AUTO_RX34 no_keyboard=1 no_CSV=1 duration=25s exact_type=1 unknown_failopen=1 pixel=OPEN");''',1)
s.write_text(v,encoding="utf-8")
print("V465 Ally log-only 25s AUTO after RX34 hit; no CSV/F9/F10; per-handle compact ReShade.log")
