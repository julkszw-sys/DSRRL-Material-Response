#!/usr/bin/env python3
"""V310 source audit: owner join MUST fail-open except exact live parser/FLVER."""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
hooks=(r/"src/runtime/flver_engine_hooks.cpp").read_text("utf-8")
header=(r/"include/dsrrl/runtime/v310_live_parser_owner_sha_join.hpp").read_text("utf-8")
slot0=(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text("utf-8")
original=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text("utf-8")
assert hooks.count("v310::reconcile_exact_slot0_owner_parser(")==1
assert "if (!identity.actual_material_exact && actual_material != nullptr)" in hooks
assert "g_v298_live_parser_mtd.lookup(actual_material,live)" in hooks
assert "live.material==actual_material" in hooks
assert "k_authored_slot0_count==16u" in slot0
for required in (
    "identity.owner_tuple_exact","identity.material_slot_valid",
    "identity.route_index!=0u","identity.actual_material_exact",
    "identity.raw_mtd_sha256==parser.raw",
    "parser.raw_bytes==0u","parser.material==nullptr",
    "v297::raw_sha_equal(parser.raw,row.dsr_raw_mtd_sha256)",
    "mr::has_exact_flver_material_ownership(query)",
    "mr::classify_mtd_envspec_semantics(query)",
    "env.envspc_slot!=0u",
    "identity=proposed",
):
    assert required in header,required
assert "if (latest_hook_source(out))" in original
assert "v299::authored_slot0_source(material)" in original
assert "v299::authored_slot0_draw(material, decision)" in (r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
assert "g_pmetal_envspec.prepare(" in (r/"integrated/integrated_addon.cpp").read_text("utf-8")
print("V310_ORIGINAL_FLVER_SHA_SLOT_LIVE_PARSER_SHA_EXACT_SOURCE_TO_CONSUMER_PASS")
print("V310_NEW_ACTIVATION_AND_PIXEL_OPEN")
