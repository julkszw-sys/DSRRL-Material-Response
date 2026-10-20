#!/usr/bin/env python3
"""V230 owner runtime t12/t1 source negative-path construction contract.
No unknown resource should be accepted by these diagnostic-only witnesses.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf-8")
build=(root/"tools/build_v230_production.ps1").read_text("utf-8")
workflow=(root.parent/".github/workflows/v230-production-from-v359.yml").read_text("utf-8")
donor=(root/"data/provenance/user_envspec_intensity_overrides_v355.tsv").read_text("utf-8")
for required in (
    "v230_trace_missing_source(",
    "[DSRRL V230 MISSING SOURCE]",
    "v358_probe_origin_for_native_view(srv,origin)",
    "v214_resource_provenance::instance().snapshot(",
    "origin.initial_data_exact",
    "origin.certified_native_probe",
    "creation_sha256",
    "action=FAIL_OPEN_NO_ALIAS",
    "context->PSGetShaderResources(binding,1u,&srv)",
    "v230_trace_missing_source(context,12u",
    "v230_trace_missing_source(context,1u",
    "g_v230_missing_source_first.fetch_or",
): assert required in src, required
helper=src[src.index("void v230_trace_missing_source("):src.index("#endif\n#endif",src.index("void v230_trace_missing_source("))]
assert helper.count("PSGetShaderResources(")==1
assert all(no not in helper for no in (
    "PSSetShaderResources(", "PSSetConstantBuffers(", "CopyResource(",
    "Draw(", "DrawIndexed(", "create_resource(", "g_native_resources[", "Map("
)), "negative-path witness must stay read-only"
assert "DSRRL_V230_FAILOPEN_DIAGNOSTICS" in build
assert "DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS" in build
assert "DSRRL_V358_EXACT_PROBE_ORIGIN" in build
assert "DSRRL_V230_STOCK_DSR_ENVDIFFUSE" in build
assert "materialize_v358_exact_lifetime_probe_origin.py" in build
assert "materialize_v359_view_scope_identity.py" in build
assert "8\ta14\tm14_LightBank.param\tc9b879a8c5e624d0\t6\t100\t170" in donor
assert "test_v230_runtime_t12_t1_origin_witness.py" in workflow
print("V230_T12_T1_OWNER_SOURCE_FORENSICS_CONTRACT_PASS diagnosis_only=1 exact_binder_guards_retained=1")
