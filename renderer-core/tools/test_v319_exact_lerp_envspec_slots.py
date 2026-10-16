#!/usr/bin/env python3
"""V319 construction guard: exact PTDE MTD slots on two witnessed Lerp paths.
Runtime/pixel evidence is independent; this does not infer visual equivalence."""
from pathlib import Path
import re

root=Path(__file__).resolve().parents[2]
router=(root/"renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp").read_text()
cpp=(root/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
build=(root/"renderer-core/tools/build_v319_ptde_envspec_slot_one_addon.ps1").read_text()
fix=cpp.split("#if defined(DSRRL_V319_PTDE_EXACT_LERP_ENVSPEC_SLOTS)",1)[1].split("#endif",1)[0]
records=[
    ("P_DullLeather[DSB]_Edge.mtd",0,1,0,
     "44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",
     "dull_edge",280),
    ("P_Metal[DSB]_Edge.mtd",2,0,5,
     "8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",
     "metal_edge",94),
]
assert "DSRRL_V319_PTDE_EXACT_LERP_ENVSPEC_SLOTS" in build
assert "DSRRL_V317_LERP_TELEMETRY" not in build
assert "family == pmetal_envspec_receiver_family::hemenvlerp" in fix
assert "owner_tuple_exact" in fix and "material_slot_valid" in fix
assert "decision.route_index == material.route_index" in fix
assert "decision.receiver_id >= 33u && decision.receiver_id <= 35u" in fix
assert "exact_identity_match" in fix and "mtd_envspec_router_state::present" in fix
assert fix.count("hashing::matches_hex(")==2
assert fix.count("mr::mtd_semantic_hash(")==2
for name,dsr_slot,ptde_slot,route,sha,label,witness in records:
    pos=router.index('"'+name+'"')
    row=router[pos:pos+520]
    assert f"envspec_router_state::present, {dsr_slot}u" in row,(name,"router")
    digest = re.search(r'\{\{((?:0x[0-9a-f]+u,?)+)\}\}',row).group(1)
    decoded=''.join(re.findall(r'0x([0-9a-f]{2})u',digest))
    assert decoded==sha,(name,"SHA drift",decoded)
    assert f'mr::mtd_semantic_hash("{name}")' in fix
    assert sha in fix
    assert f'material.route_index == {route}u' in fix
    assert f'env_semantics.envspc_slot == {dsr_slot}u' in fix
    assert f'if ({label}) env_semantics.envspc_slot = {ptde_slot}u;' in fix if label=="dull_edge" else f'else if ({label}) env_semantics.envspc_slot = {ptde_slot}u;' in fix
    for probe in (74,77):
        old=(probe*4+dsr_slot)*24576
        new=(probe*4+ptde_slot)*24576
        assert new!=old and 0<=new and new+24576<=33619968
    print(f"V319 PASS {name} route={route} {dsr_slot}->{ptde_slot} witness={witness}")
assert 'mr::mtd_semantic_hash("P_Metal[DSB].mtd")' not in fix
assert 'mr::mtd_semantic_hash("P_Leather[DSB].mtd")' not in fix
print("V319 EXACT SLOT GUARD PASS; Stable/P_Metal ordinary and all unknown IDs unchanged; pixels OPEN")
