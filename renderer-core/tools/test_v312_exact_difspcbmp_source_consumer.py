#!/usr/bin/env python3
"""V312 auditable source row → compiled PTDE material signal checks."""
import json,math,re,struct
from pathlib import Path
r=Path(__file__).resolve().parents[1]
reg=json.loads((r/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
report=json.loads((r/"data/census/v312_exact_source_compiled_difspcbmp_expansion.json").read_text("utf-8"))
slot0=(r/"include/dsrrl/runtime/v297_authored_slot0_material.hpp").read_text()
slot1=(r/"include/dsrrl/runtime/v300_exact_leather_material.hpp").read_text()
hooks=(r/"src/runtime/flver_engine_hooks.cpp").read_text()
src=(r/"src/runtime/pmetal_env_source_runtime.cpp").read_text()
draw=(r/"src/runtime/pmetal_envspec_draw_runtime.cpp").read_text()
assert report["original_mtd_router_count"]==325
assert report["total_slot0"]==36 and report["total_slot1"]==17
assert report["added_slot0"]==20 and report["added_slot1"]==11
assert report["total_mtd_source_entries"]==53
assert len(report["new_rows"])==31
assert "k_authored_slot0_count==36u" in slot0
assert "k_leather_count==17u" in slot1
assert "match->ptde_c100" in slot1 and "match->ptde_c101" in slot1
assert "match->ptde_c102" in slot1
assert "out.c101=1.5f" not in slot1 and "out.ptde_specular_power=4.0f" not in slot1
assert "k_authored_slot0_count" in hooks and "k_leather_count" in hooks
assert "if (latest_hook_source(out))" in src
assert "v299::authored_slot0_draw(material, decision)" in draw
assert "v300::draw(material, decision)" in draw
assert "source_.latest(material, source)" in draw
assert "prepared.material_resources.spec_rgb" in draw
assert "g_pmetal_envspec.prepare(" in (r/"integrated/integrated_addon.cpp").read_text()
def f32(b):return struct.unpack(">f",struct.pack(">I",int(b,16)))[0]
def number(lit):return struct.unpack(">f",struct.pack(">f",float(lit[:-1])))[0]
R0=re.compile(r'\{\s*"([^"]+)"\s*,\s*"([0-9a-f]{64})"\s*,\s*([0-9.eE+-]+f)\s*,\s*([0-9.eE+-]+f)\s*,\s*([0-9.eE+-]+f)\s*,\s*([0-9]+u)\s*\}')
R1=re.compile(r'\{\s*"([^"]+)"\s*,\s*"([0-9a-f]{64})"\s*,\s*(0x[0-9a-fA-F]+u)\s*,\s*([0-9.eE+-]+f)\s*,\s*([0-9.eE+-]+f)\s*,\s*([0-9.eE+-]+f)\s*\}')
zero=R0.findall(slot0)
one=R1.findall(slot1)
assert len(zero)==36,len(zero)
assert len(one)==17,len(one)
raw={(x["mtd_name"],x["dsr_mtd_sha256"]):x for x in reg["records"]}
assert len(raw)==325
for name,sha,c100,c101,c102,provenance in zero:
    x=raw[(name,sha)]
    assert x["envspc_slot"]==0
    assert x["state"]=="PRESENT" and x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert len(set(x["c100_f32_bits"]))==1 and len(set(x["c101_f32_bits"]))==1
    assert number(c100)==f32(x["c100_f32_bits"][0]),name
    assert number(c101)==f32(x["c101_f32_bits"][0]),name
    assert number(c102)==f32(x["c102_f32_bits"]),name
for name,sha,route,c100,c101,c102 in one:
    x=raw[(name,sha)]
    assert x["envspc_slot"]==1
    assert x["state"]=="PRESENT" and x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert number(c100)==f32(x["c100_f32_bits"][0]),name
    assert number(c101)==f32(x["c101_f32_bits"][0]),name
    assert number(c102)==f32(x["c102_f32_bits"]),name
assert len(set((name,sha) for name,sha,*_ in zero+one))==53
for forbidden in ("C_2320_Metal[DSB].mtd","C_2680_Ghost[DSB]_Add_Depth_Edge.mtd","S_o5260breathe[DSB].mtd","C_5250_Fire_DullLeather[DSB].mtd"):
    assert forbidden not in [x[0] for x in zero+one],forbidden
for row in report["new_rows"]:
    assert (row["name"],row["sha256"]) in raw
print("V312_ORIGINAL_PTDE_DIFSPCBMP_53_MTD_SOURCE_AUTHORED_SIGNAL_CONTRACT_PASS")
print("V312_DIRECT_DSR_749_NAME_RUNTIME_COVERAGE_AND_PTDE_PIXEL_STILL_OPEN")
