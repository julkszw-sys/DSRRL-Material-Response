#!/usr/bin/env python3
"""Repair two exact nonmetal Source1 producer keys; never globally ignore route."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
s=p.read_text("utf-8")
anchor="""    // This exact material now owns the next synchronized source publication.
    // Invalidate any older selector fallback before downstream decode.
    pmetal_producer_state_begin(
        material,
        epoch);"""
rep="""    // Exact raw-MTD route canonicalization: retail producer reports route=0
    // for these two MTDs while the corresponding RX34 draw consumer carries
    // the material census route. Preserve FLVER SHA / slot / MTD / epoch.
    auto source_key_material = material;
#if defined(DSRRL_V230_EXACT_NONMETAL_SOURCE_ROUTE)
    if (material.route_index == 0u && material.valid &&
        material.owner_tuple_exact && material.material_slot_valid &&
        material.actual_material_exact) {
        namespace mr = operators::material_response;
        namespace h = operators::legacy_plan::hashing;
        if (material.semantic_name_hash ==
                mr::mtd_semantic_hash("P_DullLeather[DSB].mtd") &&
            h::matches_hex(
                material.raw_mtd_sha256,
                "7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293"))
            source_key_material.route_index = 160u;
        else if (material.semantic_name_hash ==
                mr::mtd_semantic_hash("P_DullLeather[DSB]_Edge.mtd") &&
            h::matches_hex(
                material.raw_mtd_sha256,
                "44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d"))
            source_key_material.route_index = 88u;
        if (source_key_material.route_index != material.route_index) {
            static std::atomic<std::uint32_t> mapped{0u};
            if(mapped.fetch_add(1u,std::memory_order_relaxed)<12u) {
                char b[200]{};
                std::snprintf(b,sizeof(b),
                    "[DSRRL NONMETAL EXACT ROUTE] source=%u canonical=%u slot=%u mtd8=%02x%02x%02x%02x",
                    material.route_index,source_key_material.route_index,
                    material.material_slot,
                    material.raw_mtd_sha256[0],material.raw_mtd_sha256[1],
                    material.raw_mtd_sha256[2],material.raw_mtd_sha256[3]);
                reshade::log::message(reshade::log::level::warning,b);
            }
        }
    }
#endif
    // Keep the same physical source/epoch. Do not weaken any consumer gate.
    pmetal_producer_state_begin(
        source_key_material,
        epoch);"""
assert s.count(anchor)==1,("BEGIN_KEY_DRIFT",s.count(anchor))
s=s.replace(anchor,rep,1)
old="""        pmetal_producer_state_publish(
            material,
            next,
            epoch);"""
assert s.count(old)==1,("SHADOW_PUBLISH_DRIFT",s.count(old))
s=s.replace(old,old.replace("            material,","            source_key_material,"),1)
old="""    pmetal_producer_state_publish(
        material,
        next,
        epoch);"""
assert s.count(old)==1,("FINAL_PUBLISH_DRIFT",s.count(old))
s=s.replace(old,old.replace("        material,","        source_key_material,"),1)
p.write_text(s,"utf-8")
print("V230_NONMETAL_EXACT_SOURCE_ROUTE_PASS scope=dullleather+edge owner+MTD+slot")
