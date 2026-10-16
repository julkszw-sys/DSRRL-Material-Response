#!/usr/bin/env python3
"""V312: deterministic source-compiled PTDE material operator expansion.

Extend *only* the exact homologous FRPG_Phn_ColDifSpcBmp.spx consumer
family already admitted by V297/V300, across original slot 0 and 1.
Allow original authored C100/C101/power to differ per material.
No global shader/MTD family wildcard or low-LOD _L_s resource alias.

Must run AFTER all older V297..V311 materializers and tests.
"""
import json
import math
import re
import struct
from collections import Counter
from pathlib import Path

root=Path(__file__).resolve().parents[1]
registry=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert registry["source_binary"]["record_count"]==325
assert len(registry["records"])==325

def f32bits(x):
    assert isinstance(x,str) and re.fullmatch(r"0x[0-9a-fA-F]{8}",x)
    value=struct.unpack(">f",struct.pack(">I",int(x,16)))[0]
    assert math.isfinite(value) and value>=0.0 and value<=128.0
    return value

def literal(v):
    s=format(v,".9g")
    if not any(c in s for c in ".eE"):
        s+=".0"
    assert struct.unpack(">f",struct.pack(">f",float(s)))[0]==v
    return s+"f"

def signal(x):
    c100=[f32bits(v) for v in x["c100_f32_bits"]]
    c101=[f32bits(v) for v in x["c101_f32_bits"]]
    c102=f32bits(x["c102_f32_bits"])
    assert len(c100)==3 and len(c101)==3
    assert c102>0.0
    # The current v297/v300 operator exposes scalar material factors and
    # homogeneous c100 across three channels. Vector color/factor cases
    # require a separately proven vector-valued consumer.
    assert c100[0]==c100[1]==c100[2]
    assert c101[0]==c101[1]==c101[2]
    return c100[0],c101[0],c102

def eligible(x):
    name=x["mtd_name"]
    if (x["state"],x["homology_class"],x["delta_kind"])!=(
        "PRESENT","HOMOLOGOUS_SPC","EXACT_SPX"):
        return False
    if x["ptde_spx"]!=x["dsr_spx"] or not x["ptde_spx"].endswith(
        "FRPG_Phn_ColDifSpcBmp.spx"):
        return False
    if x["envspc_slot"] not in (0,1):
        return False
    # The already-proven receiver island is the *exact* DifSpcBmp shader,
    # not ghost ADD+DEPTH, animated effects or generic alpha/S effect paths.
    # These await their own shader/receiver census and complete islands.
    if not re.match(r"^(?:A\d|M(?:\d|_)|C_|P_|Ps_)",name):
        return False
    if "Ghost" in name or "Add_Depth" in name:
        return False
    try:
        signal(x)
    except (AssertionError,ValueError,OverflowError):
        return False
    sha=x["dsr_mtd_sha256"]
    return bool(re.fullmatch(r"[0-9a-f]{64}",sha))

by_pair={}
for x in registry["records"]:
    pair=(x["mtd_name"],x["dsr_mtd_sha256"])
    assert pair not in by_pair,"Duplicate name+SHA with different PTDE operators"
    by_pair[pair]=x

paths={
    0:root/"include/dsrrl/runtime/v297_authored_slot0_material.hpp",
    1:root/"include/dsrrl/runtime/v300_exact_leather_material.hpp",
}
texts={slot:p.read_text("utf-8") for slot,p in paths.items()}
old_counts={0:16,1:6}
array_start={
    0:"inline constexpr authored_slot0_row k_authored_slot0[] = {",
    1:"inline constexpr row k_leather[]={",
}
array_end={
    0:"};\ninline constexpr std::size_t k_authored_slot0_count=",
    1:"};\ninline constexpr std::size_t k_leather_count=",
}
old_entries={}
blocks={}
for slot in (0,1):
    txt=texts[slot]
    assert txt.count(array_start[slot])==1 and txt.count(array_end[slot])==1
    block=txt.split(array_start[slot],1)[1].split(array_end[slot],1)[0]
    pairs=re.findall(r'\{\s*"([^"]+)"\s*,\s*"([0-9a-f]{64})"',block)
    assert len(pairs)==old_counts[slot],(
        "V312 preimage material cohort drift",slot,len(pairs))
    assert len(pairs)==len(set(pairs))
    for pair in pairs:
        assert pair in by_pair and by_pair[pair]["envspc_slot"]==slot,(
            "Existing source material lost exact provenance",pair)
    blocks[slot]=block
    old_entries[slot]=set(pairs)

candidates={0:[],1:[]}
excluded=Counter()
for x in registry["records"]:
    pair=(x["mtd_name"],x["dsr_mtd_sha256"])
    if pair in old_entries[0] or pair in old_entries[1]:
        continue
    if not eligible(x):
        excluded[x.get("delta_kind","UNKNOWN")]+=1
        continue
    candidates[x["envspc_slot"]].append(x)

# The point is materially broader coverage, not new, guessed MTD aliases:
assert len(candidates[0])>=10, len(candidates[0])
assert len(candidates[1])>=6, len(candidates[1])
assert sum(map(len,candidates.values()))>=20
for slot in (0,1):
    assert len(set((x["mtd_name"],x["dsr_mtd_sha256"]) for x in candidates[slot]))==len(candidates[slot])
    candidates[slot].sort(key=lambda x:(int(x["index"]),x["mtd_name"]))

# Slot0: retain the 16 original PTDE operator rows and their semantic ids,
# append source IEEE754 per-cohort values. No change to V297 selector or ABI.
extra0=[]
used_ids=set(re.findall(r",([0-9]+)u\}",blocks[0]))
for x in candidates[0]:
    a,b,c=signal(x)
    provenance=0xD000+int(x["index"])
    assert provenance<0x10000 and str(provenance) not in used_ids
    extra0.append('    {"'+x["mtd_name"]+'",\n     "'+
                  x["dsr_mtd_sha256"]+'",\n     '+
                  f"{literal(a)},{literal(b)},{literal(c)},{provenance}u"+'}')
s0=texts[0]
before0=blocks[0]
after0=before0.rstrip()+",\n"+",\n".join(extra0)+"\n"
assert s0.count(before0)==1
s0=s0.replace(before0,after0,1)
assert s0.count("k_authored_slot0_count==16u")==1
s0=s0.replace("k_authored_slot0_count==16u",
              f"k_authored_slot0_count=={16+len(extra0)}u",1)

# Slot1: IMPORTANT—PTDE signal varies for environment, stone, wing etc.
# Stop using the hardcoded *Leather* .5/1.5/4.0 consumer constants.
# Each exact shader/SPX/MTD slot1 entry transports ORIGINAL authored bits.
s1=texts[1]
row_type="struct row {const char*name;const char*sha;unsigned route;};"
assert s1.count(row_type)==1
s1=s1.replace(row_type,
    "struct row {const char*name;const char*sha;unsigned route;"
    "float ptde_c100;float ptde_c101;float ptde_c102;};",1)
old_block=blocks[1]
existing_matches=re.findall(
    r'\{\s*"([^"]+)"\s*,\s*"([0-9a-f]{64})"\s*,\s*(0x[0-9A-Fa-f]+u)\s*\}',
    old_block)
assert len(existing_matches)==6,"Cannot source-compile original V300 route IDs"
new_rows=[]
used_routes=set()
for name,sha,route in existing_matches:
    x=by_pair[(name,sha)]
    assert eligible(x),"Previously-certified V300 SPX source unexpectedly drifted"
    a,b,c=signal(x)
    used_routes.add(route)
    new_rows.append('{"'+name+'","'+sha+'",'+route+','+
                    ",".join(literal(v) for v in (a,b,c))+'}')
for x in candidates[1]:
    a,b,c=signal(x)
    route=f"0x{0x54000000+int(x['index']):08x}u"
    assert route not in used_routes
    used_routes.add(route)
    new_rows.append('{"'+x["mtd_name"]+'","'+x["dsr_mtd_sha256"]+'",'+
                    route+','+",".join(literal(v) for v in (a,b,c))+'}')
s1=s1.replace(old_block,"\n"+",\n".join(new_rows)+"\n",1)
assert s1.count("k_leather_count==6u")==1
s1=s1.replace("k_leather_count==6u",
              f"k_leather_count=={len(new_rows)}u",1)
old_sig="out.c100={{0.5f,0.5f,0.5f}};out.c101=1.5f;out.ptde_specular_power=4.0f;"
new_sig=("out.c100={{match->ptde_c100,match->ptde_c100,match->ptde_c100}};"
         "out.c101=match->ptde_c101;"
         "out.ptde_specular_power=match->ptde_c102;")
assert s1.count(old_sig)==1,"V312 needs precise V300 authored signal replacement"
s1=s1.replace(old_sig,new_sig,1)
paths[0].write_text(s0,"utf-8")
paths[1].write_text(s1,"utf-8")

report={
 "schema":"dsrrl.v312.source_compiled_exact_homologous_difspcbmp",
 "original_mtd_router_count":325,
 "original_shader":"FRPG_Phn_ColDifSpcBmp.spx",
 "existing_slot0":16,"existing_slot1":6,
 "added_slot0":len(candidates[0]),
 "added_slot1":len(candidates[1]),
 "total_slot0":16+len(candidates[0]),
 "total_slot1":6+len(candidates[1]),
 "total_mtd_source_entries":22+sum(map(len,candidates.values())),
 "new_rows":[
  {"name":x["mtd_name"],"sha256":x["dsr_mtd_sha256"],
   "source_index":x["index"],"envspec_slot":slot,
   "c100_bits":x["c100_f32_bits"],"c101_bits":x["c101_f32_bits"],
   "power_bits":x["c102_f32_bits"],
   "mtg_not_pixel":"SOURCE_CANDIDATE_ONLY"}
  for slot in (0,1) for x in candidates[slot]
 ],
 "excluded_nonhomologous_or_distinct_shaders":dict(excluded),
 "positive_runtime_scope":"RX33-35 and exact original shader/FLVER+slot/MTD/sidecar, complete atomic draw",
 "excluded_ptde_low_only_logical_spec_names":544,
 "source_logical_spec_names_direct_dsr":749,
 "pixel":"OPEN",
}
out=root/"data/census/v312_exact_source_compiled_difspcbmp_expansion.json"
out.write_text(json.dumps(report,indent=2,sort_keys=True)+"\n","utf-8")
print(f"V312_SOURCE_CONSUMER_COHORT_PASS added_slot0={len(candidates[0])} "+
      f"added_slot1={len(candidates[1])} total={report['total_mtd_source_entries']}")
print("V312_749_LOGICAL_RESOURCE_DRAW_ACTIVATION_AND_PTDE_PIXELS_OPEN")
