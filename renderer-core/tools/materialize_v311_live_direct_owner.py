#!/usr/bin/env python3
"""V311 exact missing FLVER source-owner recovery; run AFTER V310.

Does not weaken stock fail-open: requires actual direct FLVER parser model,
original 19985-tuple model+slot->MTD corpus, live current MTD parser
SHA256+pointer, original V297/V300 donor, and original MTD router.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text(encoding="utf-8")
anchor='#include "dsrrl/runtime/v310_live_parser_owner_sha_join.hpp"'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\n#if defined(DSRRL_V311_DIRECT_MODEL_LIVE_MTD_OWNER)\n#include "dsrrl/runtime/v311_live_direct_owner_recovery.hpp"\n#endif',1)
anchor=''' selector_profile_end_stage(
     profile,
     owner_lookup_begin,
     profile.owner_lookup_ticks);
#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)'''
assert s.count(anchor)==1,"V311 exact owner-stage provenance anchor drift"
replacement=''' selector_profile_end_stage(
     profile,
     owner_lookup_begin,
     profile.owner_lookup_ticks);
#if defined(DSRRL_V311_DIRECT_MODEL_LIVE_MTD_OWNER) && \\
    defined(DSRRL_V298_LIVE_MTD_WITNESS) && \\
    defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
 // V310 owner log: selected exact sidecar can have missing model owner.
 // First require an actual model registered by the DSR FLVER parser. Then
 // independently bind current material pointer -> LIVE retail MTD bytes
 // and authenticate original raw full FLVER+slot+MTD against the complete
 // 19985-tuple DSR FLVER owner corpus and one unique original PTDE donor.
 // No t1/texture hash used to manufacture a missing model SHA.
 if(!owner_lookup_ok && actual_material!=nullptr &&
    material_index>=0) {
    core::sha256_digest direct_flver_sha{};
    v298::parser_mtd_entry current_mtd{};
    operators::material_response::material_identity exact_identity{};
    const bool direct_parser_hit=
        flver_identity_lookup_direct_parser_model(
            container,direct_flver_sha);
    const bool current_mtd_hit=
        g_v298_live_parser_mtd.lookup(actual_material,current_mtd) &&
        current_mtd.material==actual_material;
    if(direct_parser_hit && current_mtd_hit &&
       v311::recover_exact_owner_from_live_parsers(
           direct_flver_sha,
           static_cast<std::uint32_t>(material_index),
           actual_material,
           current_mtd,
           &operators::material_response::generated::dsr_flver_owner_mtd_hash,
           exact_identity) &&
       publish_exact_selector_identity(
           owner,actual_material,ret,r14,r15,
           selector_stack,exact_identity,&profile)) {
        static std::atomic<std::uint32_t> v311_pass{0u};
        if(v311_pass.fetch_add(1u)<8u) {
            char msg[280]{};
            std::snprintf(msg,sizeof(msg),
                "[DSRRL V311] stage=direct_parser_owner_exact"
                " original_FLVER_SHA=1 model_slot_source=1"
                " live_MTD_SHA=1 live_material_pointer=1"
                " authored_operator=1 slot=%u raw_bytes=%u"
                " draw_activation=STILL_GATED",
                static_cast<unsigned>(material_index),
                current_mtd.raw_bytes);
            reshade::log::message(reshade::log::level::info,msg);
        }
        // Direct parser-based owners bypass the long-lived selector cache:
        // every selection must independently retain full witnesses.
        selector_profile_finish(profile,selector_profile_path::owner);
        return;
    }
 }
#endif
#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)'''
s=s.replace(anchor,replacement,1)
p.write_text(s,encoding="utf-8")
print("V311_ORIGINAL_LIVE_FLVER_AND_MTD_OWNER_RECOVERY_SOURCE_PASS")
