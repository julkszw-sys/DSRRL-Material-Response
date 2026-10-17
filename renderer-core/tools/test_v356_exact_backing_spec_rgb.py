#!/usr/bin/env python3
"""V356 source and authorization contract, after ALL materializers."""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/"src/runtime/material_resource_draw_runtime.cpp").read_text("utf8")
b=(root/"tools/build_v356_exact_backing_recovery.ps1").read_text("utf8")
required=[
"DSRRL_V356_EXACT_BACKING_SRV_ALIAS",
"v356_snapshot_exact_backing(",
"v356_same_exact_view(",
"v356_unbind_named_view(",
"g_v356_backing_witnesses",
"g_v356_backing_by_view",
"GetResource(&backing)",
"GetDesc(&desc)",
"g_ambiguous_view_device.count(w.view_key)",
"known->second.logical_hash!=w.logical_hash",
"unique_hash!=w.logical_hash",
"known->second.device!=device",
"backing_key==static_cast<std::uintptr_t>(",
"source_resource.handle",
"v356_alias_ambiguous",
"v356_alias_desc_reject",
"[DSRRL V356 EXACT BACKING]",
"asset_class::specular",
"spec_equipment_allowed(",
"query.material.owner_tuple_exact",
"material_resources_.prepare_draw_requests(" if False else "prepare_draw_requests_bound(",
"v214_resource_provenance::instance().snapshot(",
]
for value in required:assert value in s,value
assert "materialize_v356_exact_backing_spec_rgb.py" in b
assert "/DDSRRL_V356_EXACT_BACKING_SRV_ALIAS" in b
assert "DSRRL_V356_EXACT_BACKING_RECOVERY.addon64" in b
# Named backing aliases cannot directly enable material-owner and shader
# consumer permission; receiver and raw MTD gates stay upstream.
assert "owner_tuple_exact" in s
assert "spec_rgb_consumer_ready" in s
assert "v356_unbind_named_view(key);" in s
# V355 owner-approved 3 donor rows are built by the existing generator.
x=(root/"data/provenance/user_envspec_intensity_overrides_v355.tsv").read_text("utf8")
assert x.count("USER_TEST_OVERRIDE")==3
print("V356_SAME_BACKING_RESOURCE_SRV_DESC_CONTRACT_PASS")
