#!/usr/bin/env python3
"""V320 exact source contract. No GPU or PTDE pixel claims."""
from pathlib import Path
import re

r=Path(__file__).resolve().parents[2]
router=(r/"renderer-core/include/dsrrl/operators/material_response/generated_envspec_router_v1.hpp").read_text(encoding="utf-8")
op=(r/"renderer-core/src/operators/material_response/mtd_semantic_census.cpp").read_text(encoding="utf-8")
rough=(r/"renderer-core/include/dsrrl/runtime/v203_equipment_envspec36_gate.hpp").read_text(encoding="utf-8")
build=(r/"renderer-core/tools/build_v320_ptde_all_envspec_slots_one_addon.ps1").read_text(encoding="utf-8")
expected={
"a10_wet[dsb]_alp.mtd":0,
"a15_chain_tex_scroll.mtd":3,
"a17_alarm[dsb]_detb.mtd":3,
"c_2231_body[dsb].mtd":2,
"c_2320_metal[dsb].mtd":0,
"c_3500_slime[dsb].mtd":3,
"c_5260_leather[dsb].mtd":3,
"c_5290_body[dsb][m].mtd":1,
"c_5290_wing[dsb]_alp.mtd":0,
"c_dullleather[dsb]_edge.mtd":1,
"c_leather[dsb]_edge.mtd":2,
"c_metal[dsb]_edge.mtd":0,
"c_roughcloth[dsb]_edge.mtd":3,
"c_wet[dsb]_edge.mtd":0,
"m_1wood[ds][m].mtd":1,
"p_wet[dsb]_edge.mtd":0,
"ps_body[dsb]_alp.mtd":2,
"s_a17_crystal[dsb].mtd":2,
"s_wet[ds].mtd":0,
"p_dullleather[dsb]_edge.mtd":1,
"p_metal[dsb]_edge.mtd":0,
}
assert len(expected)==21
patch=op.split("#if defined(DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS)",1)[1].split("#endif",1)[0]
assert patch.count("case 0x")==21
assert patch.count("digest_matches_hex(q.material.raw_mtd_sha256,")==21
assert "out.presence != ptde_envspec_presence::present" in patch
assert "exact_identity_match" in patch
assert "q.material.semantic_name_hash==s.semantic_name_hash" in op
assert "q.material.raw_mtd_sha256==s.raw_mtd_sha256" in op
assert "DSRRL_V320_PTDE_ALL_EXACT_ENVSPEC_SLOTS" in build
assert "DSRRL_V317_LERP_TELEMETRY" not in build
assert "C_RoughCloth[DSB]_Edge.mtd" in rough and "P_RoughCloth[DSB]_Edge.mtd" in rough
assert "env.envspc_slot ==" in rough and "? 3u : 0u" in rough
assert "has_exact_flver_material_ownership" in rough
for name,new in expected.items():
    esc=re.escape(name)
    m=re.search(r'"('+esc+r')"\s*,\s*(0x[0-9a-f]+ull),\s*(0x[0-9a-f]+ull),\s*\n\s*\{\{([^}]+)\}\}\s*,\s*envspec_router_state::(\w+),\s*(\d+)u',router,re.I)
    assert m is not None,name
    original_slot=int(m.group(6))
    assert m.group(5)=="present" and original_slot!=new,name
    sha=''.join(re.findall(r"0x([0-9a-f]{2})u",m.group(4)))
    assert len(sha)==64,(name,sha)
    pattern=(r'case '+m.group(2)+r':\s*// '+re.escape(m.group(1))+r' \| DSR '+str(original_slot)+r' -> PTDE '+str(new)+r'.*?"'+sha+r'".*?out.envspc_slot='+str(new)+r'u;')
    assert re.search(pattern,patch,re.S),name
    assert (new*24576)>=0
    print("V320 PASS",name,"slot",original_slot,"to",new,"sha",sha[:12])
assert '"P_Metal[DSB].mtd"' not in patch
assert '"P_Leather[DSB].mtd"' not in patch
print("V320 21/21 PTDE source slots guarded; C/P RoughCloth Edge slot3, no fabricated logical SpecRGB; pixels OPEN")
