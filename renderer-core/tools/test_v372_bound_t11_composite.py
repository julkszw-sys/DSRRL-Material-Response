#!/usr/bin/env python3
"""Contract for exact t11 identity inside the Stable combined PS draw."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/pmetal_envspec_draw_runtime.cpp"
s=p.read_text("utf-8")
start=s.index("    const bool ptde_envdiffuse_b_required =")
end=s.index("    if (!g_native_envdiffuse_logged.exchange(",start)
cut=s[start:end]
assert cut.count("envdiffuse_bound_ready=env_resources_.prepare_envdiffuse_bound(")==1
assert "pixel_srv_shadow_snapshot(cmd_list,11u,1u,t11)" in cut
assert "context->PSGetShaderResources(11u,1u,t11)" in cut
assert "if(t11[0]!=nullptr)t11[0]->Release();" in cut
assert "family==pmetal_envspec_receiver_family::stable_hemenv" in cut
assert "envdiffuse_bound_ready=env_resources_.prepare_envdiffuse(" in cut
assert "if (!envdiffuse_bound_ready) {" in cut
assert "prepared.env_resources.probe_a" not in cut[:cut.index("} else")]
assert "ptde_envdiffuse_b_required" in cut
assert s.count("core::operator_id::env_diffuse")>0
print("V372_BOUND_T11_STABLE_COMPOSITE_SOURCE_CONTRACT_PASS pixel=OPEN")
