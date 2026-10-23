#!/usr/bin/env python3
"""Recover every unambiguous, exact MTD route=0 producer using certified route seeds."""
from pathlib import Path
p=Path("renderer-core/src/runtime/pmetal_env_source_runtime.cpp")
s=p.read_text("utf-8")
anchor_include='#include "dsrrl/operators/material_response/mtd_semantic_census.hpp"'
assert s.count(anchor_include)==1
s=s.replace(anchor_include,anchor_include+'\n#include "dsrrl/operators/material_response/generated_routes_v1.hpp"',1)
anchor="""    pmetal_producer_state_begin(material, epoch);"""
rep="""    // Exact raw-MTD route canonicalization: retail producer reports route=0
    // for exact MTDs while downstream draws carry the canonical route.
    // Preserve FLVER SHA / slot / MTD / epoch.
    auto source_key_material = material;
#if defined(DSRRL_V230_EXACT_NONMETAL_SOURCE_ROUTE)
    if (material.route_index == 0u && material.valid &&
        material.owner_tuple_exact && material.material_slot_valid &&
        material.actual_material_exact) {
        namespace mr = operators::material_response;
        namespace h = operators::legacy_plan::hashing;
        // Reuse all unique SHA-verified MTD route seeds, not two names.
        bool matched = false, ambiguous = false;
        std::uint32_t expected_route = 0u;
        for (const auto &seed : mr::generated::k_material_routes_v1) {
            if (material.semantic_name_hash !=
                    mr::mtd_semantic_hash(seed.mtd_name) ||
                !h::matches_hex(material.raw_mtd_sha256, seed.sha256))
                continue;
            if (matched && expected_route != seed.route_index) {
                ambiguous = true;
                break;
            }
            matched = true;
            expected_route = seed.route_index;
        }
        if (matched && !ambiguous)
            source_key_material.route_index = expected_route;
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
old="""        pmetal_producer_state_publish(material,next,epoch);"""
assert s.count(old)==1,("SHADOW_PUBLISH_DRIFT",s.count(old))
s=s.replace(old,old.replace("material,next,epoch","source_key_material,next,epoch"),1)
old="""    pmetal_producer_state_publish(material,next,epoch);"""
assert s.count(old)==1,("FINAL_PUBLISH_DRIFT",s.count(old))
s=s.replace(old,old.replace("material,next,epoch","source_key_material,next,epoch"),1)
p.write_text(s,"utf-8")
print("V230_NONMETAL_EXACT_SOURCE_ROUTE_PASS scope=all_unique_verified_mtd_sha_name_routes owner+MTD+slot")

# Apply the matching source/draw full-identity key invariant to this same build.
import runpy
runpy.run_path('renderer-core/tools/materialize_v230_certified_producer_key.py')
