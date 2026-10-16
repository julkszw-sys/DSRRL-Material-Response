#!/usr/bin/env python3
"""V314 69 PTDE-exact SPX source rows + true shader census negative authority."""
import json,re,struct
from collections import Counter
from pathlib import Path
root=Path(__file__).resolve().parents[1]
j=json.loads((root/"data/census/v314_lit_mul_mullit_original_spx_source_census.json").read_text("utf-8"))
src=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
h=(root/"include/dsrrl/runtime/v314_original_lit_mul_mullit_sources.hpp").read_text()
probe=(root/"include/dsrrl/runtime/v314_unrouted_ps_receiver_census.hpp").read_text()
integrated=(root/"integrated/integrated_addon.cpp").read_text()
assert j["exact_source_mtds"]==69 and j["families"]=={"Lit":40,"Mul":16,"MulLit":13}
assert j["declared_operator_authorizations"]==0
assert j["direct_dsr_spec_rgb_names_target"]==749
assert j["ptde_low_lod_direct_dsr_excluded"]==544
assert len(j["entries"])==69
assert h.count('{"')==69
assert "static_assert(k_sources.size()==69u" in h
lookup={(x["mtd_name"],x["dsr_mtd_sha256"]):x for x in src["records"]}
assert len(lookup)==325
for x in j["entries"]:
    name=x["name"];sha=x["raw_mtd_sha256"]
    row=lookup[(name,sha)]
    assert row["state"]=="PRESENT" and row["delta_kind"]=="EXACT_SPX"
    assert row["homology_class"]=="HOMOLOGOUS_SPC"
    assert row["ptde_spx"]==row["dsr_spx"]
    assert row["envspc_slot"]==x["slot"]
    assert row["c100_f32_bits"]==x["c100_bits"]
    assert row["c101_f32_bits"]==x["c101_bits"]
    assert row["c102_f32_bits"]==x["power_bits"]
    assert row["ptde_spx"].endswith(x["shader_spx"])
    assert h.count('"'+name+'"')==1
    assert h.count('"'+sha+'"')==1
assert "g_shaders" in probe and "g_bound" in probe
for needle in ("source_exact=%u","ps_sha=%s","authorization=DENIED",
               "g_reported.fetch_add(1u)","observe_bind(","observe_pipeline(",
               "m.owner_tuple_exact","m.actual_material_exact",
               "m.semantic_name_hash","matches_hex(","NO_AUTHORIZED_OPERATOR",
               "native_stock_t1","g_first"):
    assert needle in probe,needle
for needle in ("v314::observe_pipeline(","v314::observe_bind(",
               "v314::forget_pipeline(","v314::report_stock_unrouted(",
               "material_owner_selection_consume(diagnostic)",
               "pixel_srv_shadow_snapshot("):
    assert needle in integrated,needle
assert integrated.count("if (route_mask == 0u) {")>=1
assert "v313::promote(" in integrated
assert "if (latest_hook_source(out))" in (root/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
for forbidden in ("v314::promote(","v314::draw(","v314::source("):
    assert forbidden not in integrated and forbidden not in probe,forbidden
print("V314_69_EXACT_PTDE_LIT_MUL_MULLIT_SOURCE_AUTHENTICITY_AND_READONLY_RECEIVER_PASS")
print("V314_NO_NEW_SHADER_AUTHORIZATION_CONTRACT_PASS; RUNTIME_PIXELS_OPEN")
