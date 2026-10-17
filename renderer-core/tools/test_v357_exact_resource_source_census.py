#!/usr/bin/env python3
"""V357 compile-unit source contract after ALL source materializers.
Verify instrumentation is passive and provider/consumer identity guards stay.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=(root/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text("utf8")
q=(root/"src/runtime/envspec_resource_runtime.cpp").read_text("utf8")
b=(root/"tools/build_v357_exact_resource_source_census.ps1").read_text("utf8")
for marker in (
    "[DSRRL V357 RESOURCE SOURCE]",
    "v357_trace_rejected_srv(",
    "PSGetShaderResources(binding,1u,&srv)",
    "srv->GetResource(&resource)",
    "srv->GetDevice(&device)",
    "QueryInterface(",
    "v214_resource_name_provenance::instance().snapshot(",
    "NO_PRODUCER_WITNESS",
    "AMBIGUOUS_SOURCE",
    "EXACT_RETAIL_SOURCE",
    "backing_identity=OBSERVED_NOT_ASSIGNED",
    "DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS",
    "[DSRRL V354 PROBE REJECT]",
    "[DSRRL V354 SPEC REJECT]",
):
    assert marker in p,marker
for marker in (
    "[DSRRL V357 BC6H PRODUCER]",
    "stage=sha_not_in_certified_atlas",
    "stage=initial_data_not_extractable",
    "digest[i]>>4u",
    "env::legacy_native_probe_for_sha(",
    "g_v357_unknown_bc6h_census",
    "DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS",
):
    assert marker in q,marker
assert p.count("v357_trace_rejected_srv(")==4 # one declaration, three calls
assert q.count("[DSRRL V357 BC6H PRODUCER]")==2
# Fail-open branches still present and unpatched. Readback creates no GPU writes.
for marker in ("if (!env_resource_ready) {","if (!material_ready ||","return false;"):
    assert marker in p
assert not any(token in p[p.index("void v357_trace_rejected_srv("):p.index("void log_prepare_stage_once(")] for token in (
    "PSSetShaderResources(","PSSetConstantBuffers(","CopyResource(","Map(","DrawIndexed("
))
assert "/DDSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS" in b
assert "materialize_v357_exact_resource_source_census.py" in b
assert "DSRRL_V357_EXACT_RESOURCE_SOURCE_CENSUS.addon64" in b
t=(root/"data/provenance/user_envspec_intensity_overrides_v355.tsv").read_text("utf8")
assert t.count("USER_TEST_OVERRIDE")==3
print("V357_NEGATIVE_SOURCE_PROVENANCE_CONTRACT_PASS source_only=YES routing_unchanged=YES")
