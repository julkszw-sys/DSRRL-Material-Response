#!/usr/bin/env python3
"""V459: append typed RX34 Light pair to existing V451 kind21; no additional event.
No new EXE hook, draw replay, shader/CB/asset mutation or row/source selection.
The read happens ONLY in V450 manual F9, RX34 route345 rows30/31 scope.
"""
from pathlib import Path

R=Path("renderer-core")
integrated=R/"integrated/integrated_addon.cpp"
source=R/"src/runtime/pmetal_env_source_runtime.cpp"
script=integrated.read_text(encoding="utf-8")
needle="""        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V451_MODEL_RESOURCE_AT_ISSUE,{
            rx447.native_sequence,static_cast<unsigned>(prepared.source_kind),
            rx447.row_a,issued448.w.entity,issued448.w.dp,
            issued448.w.engine_model,resource451,controller451,
            issued448.w.packet,issued448.w.frame_rbp,
            resource_ok451?1u:0u,controller_ok451?1u:0u,
            static_cast<unsigned>(result450.state),
            static_cast<unsigned>(issued448.status),
            material.material_slot,material.route_index,
            rx447.context});"""
addition="""        // V459: native typed FrpgModelDrawEntity+0x108 FrpgDrawParam.
        // Only observe exact class identities from SHA-locked retail RE.
        // If any check fails, report its status, NEVER infer a Light pair.
        // No additional hooks, events, eligibility gate or renderer mutation.
        const auto base459=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto entity459=static_cast<std::uintptr_t>(issued448.w.entity);
        const auto dp459=static_cast<std::uintptr_t>(issued448.w.dp);
        std::uintptr_t entity_vt459=0u,dp_vt459=0u;
        std::uint16_t light_current459=0u,light_target459=0u;
        std::uint32_t light_alpha459=0u,light_duration459=0u,light_elapsed459=0u;
        std::uint32_t light_status459=0u;
        if(valid_builder451 && base459 && entity459) {
            light_status459|=1u;
            if(dsrrl::runtime::v443::read(entity459,entity_vt459) &&
               entity_vt459==base459+0x12ECA10u) {
                light_status459|=2u;
                if(dsrrl::runtime::v443::read(entity459+0x108u,dp_vt459) &&
                   dp_vt459==base459+0x12E5C08u) {
                    light_status459|=4u;
                    if(dp459==entity459+0x108u)light_status459|=8u;
                    if(dsrrl::runtime::v443::read(entity459+0x110u,light_current459) &&
                       dsrrl::runtime::v443::read(entity459+0x112u,light_target459) &&
                       dsrrl::runtime::v443::read(entity459+0x114u,light_alpha459)) {
                        light_status459|=16u;
                        if(dsrrl::runtime::v443::read(entity459+0x118u,light_duration459) &&
                           dsrrl::runtime::v443::read(entity459+0x11Cu,light_elapsed459))
                            light_status459|=32u;
                    }
                }
            }
        }
        // V451 has 17 fields; use its remaining 7 slots to avoid increasing
        // capped event rate or producing an unjoinable second event.
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V451_MODEL_RESOURCE_AT_ISSUE,{
            rx447.native_sequence,static_cast<unsigned>(prepared.source_kind),
            rx447.row_a,issued448.w.entity,issued448.w.dp,
            issued448.w.engine_model,resource451,controller451,
            issued448.w.packet,issued448.w.frame_rbp,
            resource_ok451?1u:0u,controller_ok451?1u:0u,
            static_cast<unsigned>(result450.state),
            static_cast<unsigned>(issued448.status),
            material.material_slot,material.route_index,
            rx447.context,
            light_status459,light_current459,light_target459,
            light_alpha459,light_duration459,light_elapsed459,
            entity_vt459});"""
assert script.count(needle)==1,("V451 emit preimage drift",script.count(needle))
integrated.write_text(script.replace(needle,addition,1),encoding="utf-8")
s=source.read_text(encoding="utf-8")
needle2="""        "[DSRRL V451] MODEL_RESOURCE_ISSUED_SHADOW entity_plus_20_read_only=1 actor_generation=OPEN patch=OFF");"""
assert s.count(needle2)==1,("V451 marker drift",s.count(needle2))
s=s.replace(needle2,needle2+"""
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V459] RX34_TYPED_LIGHT_PAIR_INLINE_V451_EVENT F9_8s MAX1024 patch=OFF owner=OPEN pixel=OPEN");""",1)
source.write_text(s,encoding="utf-8")
print("V459 PASS: 1 existing kind21 event extended 17->24; read-only typed Light current/target/alpha/time; no new hooks")
