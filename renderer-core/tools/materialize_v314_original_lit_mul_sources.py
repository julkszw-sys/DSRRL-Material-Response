#!/usr/bin/env python3
"""V314 original PTDE exact SPX family census, NOT shader authorization.

Compile all original 325-router exact PTDE/DSR Lit/Mul/MulLit
DifSpcBmp variants to an immutable source-MTD family lookup. Never
claim a receiver from SPX or an MTD name; actual GPU shader SHA
must be proven in the game before authorizing any future operator.
"""
import json,re,struct,math
from collections import Counter
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert source["source_binary"]["record_count"]==325
classes={
 "Lit":("FRPG_Phn_ColDifSpcBmpLit.spx",1),
 "Mul":("FRPG_Phn_ColDifSpcBmpMul.spx",2),
 "MulLit":("FRPG_Phn_ColDifSpcBmpMulLit.spx",3),
}
entries=[];seen=set();counts=Counter()
def scalar_bits(items):
    assert len(items)==3
    v=[int(i,16) for i in items]
    assert all(math.isfinite(struct.unpack(">f",struct.pack(">I",n))[0]) for n in v)
    return v
for row in source["records"]:
    for family,(suffix,enum) in classes.items():
        if not row["ptde_spx"].endswith(suffix):
            continue
        if not (
          row["state"]=="PRESENT" and
          row["homology_class"]=="HOMOLOGOUS_SPC" and
          row["delta_kind"]=="EXACT_SPX" and
          row["ptde_spx"]==row["dsr_spx"]):
            break
        assert row["envspc_slot"] in (0,1,2,3)
        sha=row["dsr_mtd_sha256"]
        assert re.fullmatch("[0-9a-f]{64}",sha)
        key=(row["mtd_name"],sha)
        assert key not in seen
        seen.add(key)
        c100=scalar_bits(row["c100_f32_bits"])
        c101=scalar_bits(row["c101_f32_bits"])
        power=int(row["c102_f32_bits"],16)
        assert math.isfinite(struct.unpack(">f",struct.pack(">I",power))[0])
        entries.append(dict(name=row["mtd_name"],raw_mtd_sha256=sha,
            source_index=row["index"],family=family,shader_spx=suffix,
            family_id=enum,slot=row["envspc_slot"],
            c100_bits=row["c100_f32_bits"],c101_bits=row["c101_f32_bits"],
            power_bits=row["c102_f32_bits"]))
        counts[family]+=1
        break
assert dict(counts)=={"Lit":40,"Mul":16,"MulLit":13},counts
assert len(entries)==69
entries.sort(key=lambda x:x["source_index"])
cxx=['#pragma once','// V314 original MTD source identities only. NEVER a shader gate.',
 '#include <array>','#include <cstdint>','#include <cstddef>',
 'namespace dsrrl::runtime::v314 {',
 'struct source_row { const char* name; const char* sha; ',
 '  std::uint32_t family,slot,original_index;',
 '  std::array<std::uint32_t,3> c100,c101; std::uint32_t power; };',
 'inline constexpr std::array<source_row,69> k_sources={{']
for e in entries:
    n=lambda val:str(int(val,16))+"u"
    aa=",".join(n(v) for v in e["c100_bits"])
    bb=",".join(n(v) for v in e["c101_bits"])
    cxx.append('{"'+e["name"]+'","'+e["raw_mtd_sha256"]+'",'+str(e["family_id"])+'u,'+
      str(e["slot"])+'u,'+str(e["source_index"])+'u,{{'+aa+'}},{{'+bb+'}},'+
      n(e["power_bits"])+'},')
cxx+=['}};','static_assert(k_sources.size()==69u,"source census drift");','}']
(root/"include/dsrrl/runtime/v314_original_lit_mul_mullit_sources.hpp").write_text(
 "\n".join(cxx)+"\n","utf-8")
report={"schema":"dsrrl.v314.original_exact_spx_non_authorizing_receiver_frontier",
 "original_registry_rows":325,"families":dict(counts),"exact_source_mtds":69,
 "declared_operator_authorizations":0,"gpu_ps_sha_census":"REQUIRED",
 "exact_receiver_to_spx_family":"UNCONFIRMED",
 "ptde_low_lod_direct_dsr_excluded":544,"direct_dsr_spec_rgb_names_target":749,
 "per_draw_atom":"NOT_GRANTED","runtime":"OPEN","pixel":"OPEN",
 "entries":entries}
(root/"data/census/v314_lit_mul_mullit_original_spx_source_census.json").write_text(
 json.dumps(report,indent=2,sort_keys=True)+"\n","utf-8")
print("V314_ORIGINAL_HOMOLOGOUS_SPX_SOURCE_CENSUS_PASS Lit=40 Mul=16 MulLit=13 exact=69")
print("V314_RECEIVER_NOT_AUTHORIZED_GUARD=PASS pixel=OPEN")
