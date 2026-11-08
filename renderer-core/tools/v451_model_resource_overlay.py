#!/usr/bin/env python3
"""V451 read-only model-resource identity from issued native RX34, AFTER V450.
Never modifies source selection, GPU state, row values, shaders or R43.
No new EXE hooks, no selector unwind, no background CSV capture.
"""
from pathlib import Path
R=Path('renderer-core')
t=R/'include/dsrrl/runtime/v438_one_shot_trace.hpp'
i=R/'integrated/integrated_addon.cpp'
s=R/'src/runtime/pmetal_env_source_runtime.cpp'
def rep(path,old,new):
 x=path.read_text(encoding='utf-8')
 if x.count(old)!=1:
  raise RuntimeError(f'V451 drift in {path}: {x.count(old)} instances of {old[:90]!r}')
 path.write_text(x.replace(old,new,1),encoding='utf-8')

rep(t,'V450_RX34_NATIVE_JOIN=20','V450_RX34_NATIVE_JOIN=20,V451_MODEL_RESOURCE_AT_ISSUE=21')
rep(t,'t!=V448_ISSUED_FRAME && t!=V450_RX34_NATIVE_JOIN &&',
      't!=V448_ISSUED_FRAME && t!=V450_RX34_NATIVE_JOIN &&\n    t!=V451_MODEL_RESOURCE_AT_ISSUE &&')
# This is within the V450 manual F9/8 sec and exact RX34/route345 scope.
# V443::read validates readability; no pointer returned by a failed issued
# builder probe is dereferenced, and 0x50 is treated as optional opaque data.
needle='''        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V450_RX34_NATIVE_JOIN,{
            rx447.native_sequence,rx447.context,rx447.row_a,
            static_cast<unsigned>(result450.state),
            binding447.producer.entity,issued448.w.entity,
            binding447.producer.packet,issued448.w.packet,
            binding447.producer.model,issued448.w.engine_model,
            issued448.w.frame_rbp,material.material_slot,
            material.route_index,static_cast<unsigned>(issued448.status)});'''
addition=needle+'''
        // V451: lookup token created by native FrpgDrawEntity base ctor.
        // This is a model-resource pointer, NOT a gameplay actor ID.
        std::uintptr_t resource451=0u, controller451=0u;
        const bool valid_builder451=issued448.w.valid &&
            issued448.status==dsrrl::runtime::v448::probe_status::live_default_model;
        const bool resource_ok451=valid_builder451 &&
            dsrrl::runtime::v443::read(issued448.w.entity+0x20u,resource451) &&
            resource451!=0u;
        const bool controller_ok451=valid_builder451 &&
            dsrrl::runtime::v443::read(issued448.w.entity+0x50u,controller451);
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V451_MODEL_RESOURCE_AT_ISSUE,{
            rx447.native_sequence,static_cast<unsigned>(prepared.source_kind),
            rx447.row_a,issued448.w.entity,issued448.w.dp,
            issued448.w.engine_model,resource451,controller451,
            issued448.w.packet,issued448.w.frame_rbp,
            resource_ok451?1u:0u,controller_ok451?1u:0u,
            static_cast<unsigned>(result450.state),
            static_cast<unsigned>(issued448.status),
            material.material_slot,material.route_index,
            rx447.context});
'''
rep(i,needle,addition)
needle='''        "[DSRRL V450] MANUAL_RX34_LIGHT_AUDIT F9_8s MAX1024_EVENTS auto_csv=OFF selector_unwind=OFF patch=OFF");'''
rep(s,needle,needle+'''
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V451] MODEL_RESOURCE_ISSUED_SHADOW entity_plus_20_read_only=1 actor_generation=OPEN patch=OFF");''')
print('V451 source overlay PASS: manual F9 RX34 resource identity read, no rendering mutations')
