#!/usr/bin/env python3
"""V322 exact source guard. PTDE-vs-DSR BND3 regression:
entry starts at 0x20 (not 0x2c); flags,length,offset,id,name_offset,decompressed.
Previous V319/V320 PTDE EnvSpec remaps are invalid and may not ship.
This test is static and does not instrument draw timings.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
router=(root/"renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp").read_text()
operator=(root/"renderer-core/src/operators/material_response/mtd_semantic_census.cpp").read_text()
draw=(root/"renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
rough=(root/"renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp").read_text()
build=(root/"renderer-core/tools/build_v322_correct_bnd3_rollback_one_addon.ps1").read_text()
for marker in ("DSRRL_V319_PTDE_EXACT_LERP_ENVSPEC_SLOTS","DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS"):
    assert marker not in operator and marker not in draw and marker not in rough and marker not in build,marker
assert "env.envspc_slot == 0u;" in rough
assert "return envspec_from_router_record(s);" in operator
assert "v322_correct_bnd3_rollback" in build
assert "DSRRL_V317_LERP_TELEMETRY" not in build
# Exact 26 original BND3 DSR named-payload SHA256 fingerprints (correct
# 0x20 record origin; 0x2c creates a one-payload off-by-one mismatch).
pairs={
"A01[D]_Alp.mtd":"89076b688246989ed9d7c65da4df4508fd2c1ec13c8f929fd1ec852507f7e4ad",
"A02_C5000_Slime[DSB]_Alp.mtd":"3419cf5db5e292a1a03434396349d3bebc9ae68dc23277d71724a7fc5b631317",
"A03_1Wood[DSB][L].mtd":"66bab9da9dda0e708f2a45004f658fcf32ca0470e5b7cdf722018c35b35206f1",
"A03_4Stone[DSB][L].mtd":"4f0cc6f4946abf397d64fd0241023f152dd2d1447bd52662cc8614409e2cf074",
"A03_4Stone[DSB][ML].mtd":"cd29369ef71f08192301e7b3158d62a0b74179d2ac0682d3478ba2e2bd0774fc",
"C_Leather[DSB].mtd":"236bb0a0614e4bf3326d610915f8b22c3cacb09ee1c0523a51fa08200dfbe524",
"C_Leather[DSB]_Alp.mtd":"9ef69c874a7ceff3797a3513026677d131f622f3f8476015a5ce4b70fcd6e190",
"C_Metal[DSB].mtd":"ae2e8df867fe2859eef37104c13c939e7e7fe4f703b7fe49c408d0230fc71d85",
"C_Metal[DSB]_Alp.mtd":"45eb950965998f67d2d2b62fbcfc841f07e71e24097e1c6ceb97758752a651f0",
"C_Metal[DSB]_Edge.mtd":"f797c63ac4f476ab7ebb7bc150ecc2a9c61851999f5051ceb864b62c95d924d9",
"P_DullLeather[DSB].mtd":"7ea7d6ce9bb7148db62e8c27ee45edefb9547018d0b723dd4753d4c37e29f293",
"P_DullLeather[DSB]_Edge.mtd":"44e1188bafa99d49349883dcc2052c13365f196e341ca5e7c788cbad0a4e058d",
"P_Leather[DSB].mtd":"0f2b9a1012b83c4dfc909c311d29a11147de8c15191bc62398ce0d5e1cc6757e",
"P_Leather[DSB]_Alp.mtd":"4c728a9b5957a75d0eb82b2b77b800829e1973632c7c0690a7e31a074c85e7fb",
"P_Leather[DSB]_Edge.mtd":"cf968a22722667777d10429a03a2e4a87f867412949a7fe5590cb8cefaa2bfa4",
"P_Leather[DS].mtd":"53819ead337c1ecd8593d8535c1fc1fdde589f00eb8ea015a8ae347da71073ea",
"P_Metal[DSB].mtd":"ece70f36bd2517d28c8495e276cea537f8b519d6bed981788e79a409ffbf763b",
"P_Metal[DSB]_Alp.mtd":"45b985a46381199775b0089e9ac6ad5ea14a97b215421bfe29c29bd7ad97535a",
"P_Metal[DSB]_Edge.mtd":"8b730ce8655401c5cae694ba5535efb088d9d67db2a157d8f142cb5629293485",
"P_RoughCloth[DSB].mtd":"db197a28abd96f55ce54da65dfea74e8788464dd6ae13ca7ceee038c56d281b1",
"P_RoughCloth[DSB]_Edge.mtd":"4b3451c1d83d31f5ac1aad11bb5a16985ce49928bca3fcc10005032c784e45b8",
"P_Wet[DSB].mtd":"7af76c9ed5adbb22a574b2afa382a10dfec37dc17470a3d9ea47be3bfb1ea97b",
"P_Wet[DSB]_Edge.mtd":"a7f73fd656797fea6eeb59131a5bd5c25198ad5c654524dadf76798c57aebb86",
"Ps_Body[DSB].mtd":"af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95",
"Ps_Body[DSB][M].mtd":"d96f22bb5c1127e1e5b08dd79f4062c9e695ffbbd10e11827405dc1889e91842",
"Ps_Body[DSB]_Alp.mtd":"024f15bad3c22cd7476c82229b429eb81c3ea01626c18f8887e98f802f390aab",
}
assert len(pairs)==26
records=re.findall(r'"([^"\n]+\.mtd)",\s*0x[0-9a-f]+ull,\s*0x[0-9a-f]+ull,\s*\{\{([^}]+)\}\},\s*envspec_router_state::(\w+),\s*(\d+)u',router,re.I)
assert len(records)==325,len(records)
record_map={}
for name,sha_bytes,state,slot in records:
    digest=''.join(re.findall(r'0x([0-9a-f]{2})u',sha_bytes,re.I))
    record_map.setdefault(name.lower(),[]).append((digest,state,int(slot)))
for name,sha in pairs.items():
    assert any(x[0]==sha for x in record_map[name.lower()]),(name,'invalid named SHA')
for name,slot in {"P_DullLeather[DSB]_Edge.mtd":0,"P_Metal[DSB]_Edge.mtd":2,"C_RoughCloth[DSB]_Edge.mtd":0,"P_Metal[DSB].mtd":2,"P_Leather[DSB].mtd":1}.items():
    assert any(x[2]==slot and x[1]=="present" for x in record_map[name.lower()]),name
print("V322 PASS 26/26 exact named MTD SHA; 325 router entries; original PTDE/DSR slot guards restored.")
print("Note: 325/325 validated separately against original BND3 offline; runtime pixels and FPS OPEN.")
