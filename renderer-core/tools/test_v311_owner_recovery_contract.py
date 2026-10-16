#!/usr/bin/env python3
"""V311 exact source-model/live-MTD owner, no global name gate."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
h=(r/"include/dsrrl/runtime/v311_live_direct_owner_recovery.hpp").read_text()
s=(r/"src/runtime/flver_engine_hooks.cpp").read_text()
donor0=(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text()
donor1=(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text()
consumer=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
assert "k_authored_slot0_count==16u" in donor0
assert "k_leather_count==6u" in donor1
for x in (
    "live.material!=selected_material", "live.raw_bytes==0u",
    "owner_mtd_hash(model_sha,slot,source_semantic)",
    "v297::raw_sha_equal(live.raw,row.dsr_raw_mtd_sha256)",
    "v297::raw_sha_equal(live.raw,row.sha)",
    "matched!=1", "mr::has_exact_flver_material_ownership(q)",
    "mr::classify_mtd_envspec_semantics(q)",
    "env.envspc_slot!=exact_env_slot",
    "candidate.flver_sha256=model_sha",
    "candidate.raw_mtd_sha256=live.raw",
    "candidate.actual_material_exact=true",
):
    assert x in h,x
for x in (
    "flver_identity_lookup_direct_parser_model(",
    "g_v298_live_parser_mtd.lookup(actual_material,current_mtd)",
    "current_mtd.material==actual_material",
    "&operators::material_response::generated::dsr_flver_owner_mtd_hash",
    "v311::recover_exact_owner_from_live_parsers(",
    "publish_exact_selector_identity(",
    "selector_profile_finish(profile,selector_profile_path::owner)",
):
    assert x in s,x
assert "if (latest_hook_source(out))" in (r/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
assert "v299::authored_slot0_draw(material, decision)" in consumer
assert "v300::draw(material, decision)" in consumer
assert "source_.latest(material, source)" in consumer
assert "g_pmetal_envspec.prepare(" in (r/"integrated/integrated_addon.cpp").read_text()
print("V311_MODEL_SHA_SLOT_SOURCE_AND_LIVE_MTD_ATOMIC_POSITIVE_GATE_PASS")
print("V311_NEW_RUNTIME_AND_PIXEL_OPEN")
