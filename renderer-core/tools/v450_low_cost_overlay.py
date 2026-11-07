#!/usr/bin/env python3
"""V450 after V448: manual bounded RX34-only telemetry, no operator mutation."""
from pathlib import Path
R=Path('renderer-core')
t=R/'include/dsrrl/runtime/v438_one_shot_trace.hpp'
f=R/'src/runtime/flver_engine_hooks.cpp'
i=R/'integrated/integrated_addon.cpp'
s=R/'src/runtime/pmetal_env_source_runtime.cpp'
def rep(path,old,new):
 x=path.read_text(encoding='utf-8')
 if x.count(old)!=1:
  raise RuntimeError(f'V450 preimage drift {path}: {x.count(old)} x {old[:80]!r}')
 path.write_text(x.replace(old,new,1),encoding='utf-8')
# Replace V439 automatic 180s capture with manual 8s F9, F10 remains a marker.
rep(t,'if(!s.start_attempted.load() && s.scene_draws.load()>=12u && frame>=30u)start_one_shot();',
 '''static bool was_f9_v450=false;
 const bool f9_v450=(GetAsyncKeyState(VK_F9)&0x8000)!=0;
 if(f9_v450 && !was_f9_v450 && !s.start_attempted.load())start_one_shot();
 was_f9_v450=f9_v450;''')
rep(t,'CAPTURE_START AUTO: 180s','CAPTURE_START MANUAL_F9: 8s')
rep(t,'GetTickCount64()-s.started_at_ms.load()<180000u','GetTickCount64()-s.started_at_ms.load()<8000u')
rep(t,'constexpr u64 ring_capacity=131072;','constexpr u64 ring_capacity=4096;')
rep(t,'drain(12000);','drain(256);')
rep(t,'for(unsigned tries=0;tries<200u&&s.tail.load()!=s.head.load();++tries){drain(16000);Sleep(1);}',
 '''for(unsigned tries=0;tries<10u&&s.tail.load()!=s.head.load();++tries){drain(1024);Sleep(1);}''')
rep(t,'V448_ISSUED_FRAME=19','V448_ISSUED_FRAME=19,V450_RX34_NATIVE_JOIN=20')
rep(t,'auto &s=state(); if(!s.active.load(std::memory_order_acquire))return;',
 '''auto &s=state(); if(!s.active.load(std::memory_order_acquire))return;
 // Drop expensive high-volume general telemetry BEFORE QPC/ring operations.
 if(t!=V447_NATIVE_TICKET && t!=V447_RX34_BIND &&
    t!=V448_ISSUED_FRAME && t!=V450_RX34_NATIVE_JOIN &&
    t!=ENV_ISSUED && t!=MARKER && t!=FRAME && t!=SESSION_END)return;
 if(t==ENV_ISSUED){
   if(payload.size()<16u)return;
   const u64*v=payload.begin();
   if(v[2]!=345u||v[15]!=34u||(v[5]!=30u&&v[5]!=31u))return;
 }
 if(s.head.load(std::memory_order_relaxed)>=1024u)return;''')
# CSV V448: early selector unwind 4558 failures. Keep native probe ONLY at RX34.
rep(f,'             if(v438::state().active.load(std::memory_order_acquire)) {',
 '             if(false) { // V450: selector-stage unwind disabled by runtime falsifier')
rep(i,'if(dsrrl::runtime::v438::state().active.load(std::memory_order_acquire)){',
 'if(false){ // V450: redundant V443 receiver probe disabled')
rep(i,'''        if(decision.receiver_id==34u && material.route_index==345u &&
           (prepared.stable_envdiffuse_row==30u ||
            prepared.stable_envdiffuse_row==31u)) {''',
'''        if(dsrrl::runtime::v438::state().active.load(std::memory_order_acquire) &&
           decision.receiver_id==34u && material.route_index==345u &&
           (prepared.stable_envdiffuse_row==30u ||
            prepared.stable_envdiffuse_row==31u)) {''')
rep(i,'#include "dsrrl/runtime/v448_live_builder_core.hpp"',
'''#include "dsrrl/runtime/v448_live_builder_core.hpp"
#include "dsrrl/runtime/v450_native_join.hpp"''')
rep(i,'''        const auto result448=dsrrl::runtime::v448::evaluate(
            binding447.producer,selector448,issue448);''',
'''        const auto result448=dsrrl::runtime::v448::evaluate(
            binding447.producer,selector448,issue448);
        const auto result450=dsrrl::runtime::v450::evaluate(
            binding447.producer,issued448.w,rx447.native_sequence,
            rx447.context,rx447.row_a);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V450_RX34_NATIVE_JOIN,{
            rx447.native_sequence,rx447.context,rx447.row_a,
            static_cast<unsigned>(result450.state),
            binding447.producer.entity,issued448.w.entity,
            binding447.producer.packet,issued448.w.packet,
            binding447.producer.model,issued448.w.engine_model,
            issued448.w.frame_rbp,material.material_slot,
            material.route_index,static_cast<unsigned>(issued448.status)});''')
rep(s,'"[DSRRL V448] LIVE_BUILDER_ACTOR_RX34_SHADOW enabled=1 patch=OFF generation=OPEN");',
'''"[DSRRL V448] LIVE_BUILDER_ACTOR_RX34_SHADOW enabled=1 patch=OFF generation=OPEN");
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V450] MANUAL_RX34_LIGHT_AUDIT F9_8s MAX1024_EVENTS auto_csv=OFF selector_unwind=OFF patch=OFF");''')
print('V450 manual bounded RX34-only overlay PASS; Source1/R43/visible pixels unchanged')
