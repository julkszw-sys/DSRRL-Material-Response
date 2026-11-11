#!/usr/bin/env python3
"""V463: owner/handle identity in EXISTING issued RX34 V451 kind21 event.
Uses exact manager and owner type vtables from SHA-locked original DSR RE.
No new hook, no more Light A/B capture, no draw/source/row/CB/shader mutation.
F9 manual 8 seconds ≤1024 events, exact RX34 route345 rows 30/31 only.
"""
from pathlib import Path

root=Path("renderer-core")
i=root/"integrated/integrated_addon.cpp"
s=root/"src/runtime/pmetal_env_source_runtime.cpp"
def repl(path,old,new):
 x=path.read_text(encoding="utf-8")
 assert x.count(old)==1,(str(path),x.count(old),old[:110])
 path.write_text(x.replace(old,new,1),encoding="utf-8")

needle="""        // V451 has 17 fields; use its remaining 7 slots to avoid increasing
        // capped event rate or producing an unjoinable second event.
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V451_MODEL_RESOURCE_AT_ISSUE,{"""
addon="""        // V463 exact owner/handle join (read-only). Status bits attest
        // individual typed inputs and never veto a game draw or modify a row.
        // WorldChrManImp global+0x68 is a CANDIDATE actor pointer:
        // classify as PlayerIns ONLY if its concrete vtable matches.
        std::uintptr_t world463=0u,world_vt463=0u;
        std::uintptr_t actor463=0u,actor_vt463=0u;
        std::uintptr_t model463=0u,model_vt463=0u;
        std::uint32_t actor_handle463=0u,issued_handle463=0u;
        std::uint32_t owner_status463=0u;
        if(valid_builder451 && base459 && entity459 &&
            dsrrl::runtime::v443::read(entity459+0x10u,issued_handle463)) {
            owner_status463|=1u; // complete native receiver registry handle read
        }
        if(base459 &&
            dsrrl::runtime::v443::read(base459+0x1C77E50u,world463) &&
            world463 && dsrrl::runtime::v443::read(world463,world_vt463) &&
            world_vt463==base459+0x1328210u) {
            owner_status463|=2u; // exact typed WorldChrManImp singleton
            if(dsrrl::runtime::v443::read(world463+0x68u,actor463) &&
               actor463 && dsrrl::runtime::v443::read(actor463,actor_vt463)) {
                owner_status463|=4u; // candidate live polymorphic actor
                if(actor_vt463==base459+0x13251F0u) {
                    owner_status463|=8u; // exact PlayerIns TYPE, not inferred
                    if(dsrrl::runtime::v443::read(actor463+0x60u,model463) &&
                       model463 && dsrrl::runtime::v443::read(model463,model_vt463) &&
                       model_vt463==base459+0x12EAF30u) {
                        owner_status463|=16u; // exact ChrModel TYPE
                        if(dsrrl::runtime::v443::read(model463+0x10u,actor_handle463)) {
                            owner_status463|=32u;
                            if((owner_status463&1u) && actor_handle463==issued_handle463)
                                owner_status463|=64u; // complete 32-bit handle equality
                        }
                    }
                }
            }
        }
        // V451 original 17 fields retained; V463 v17..v23 supersedes V459
        // redundant Light-current/target/weight slots IN THIS BINARY ONLY.
        dsrrl::runtime::v438::emit(dsrrl::runtime::v438::V451_MODEL_RESOURCE_AT_ISSUE,{"""
repl(i,needle,addon)
old="""            light_status459,light_current459,light_target459,
            light_alpha459,light_duration459,light_elapsed459,
            entity_vt459});"""
new="""            owner_status463,issued_handle463,actor463,
            actor_vt463,model463,actor_handle463,world463});"""
repl(i,old,new)
note="""        "[DSRRL V459] RX34_TYPED_LIGHT_PAIR_INLINE_V451_EVENT F9_8s MAX1024 patch=OFF owner=OPEN pixel=OPEN");"""
repl(s,note,note+"""
    reshade::log::message(reshade::log::level::warning,
        "[DSRRL V463] PLAYERINS_TO_RX34_NATIVE_HANDLE_JOIN manual_F9_8s no_new_hook no_row_patch exact_vtable pixel_OPEN");""")
print("V463 PASS: same one event; 7 spare slots now certify typed world player model and complete native entity handle")
