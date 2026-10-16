#!/usr/bin/env python3
"""V331 source-only regression for Wanderer Coat 9530 metadata.

Source evidence was reconstructed from SHA-pinned original compact DSR FLVER
and PTDE owner ZIP in the project Library. Does not substitute this JSON
for authenticating any live runtime FLVER or GPU pixel output.
"""
from __future__ import annotations
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DOC=ROOT/"data/census/v331_wanderer_9530_exact_source_owner_witness_v1.json"
ROUTER=ROOT/"data/census/ptde_mtd_envspec_router_v1.json"
def main():
 d=json.loads(DOC.read_text(encoding="utf-8"))
 assert d["dsr_manifest_full_flver_sha256"]=="1398eec9e1545fb52688c691d1ff3caa042de3750bccc6b267165a136c3ca845"
 assert d["dsr_manifest_struct_sha256"]=="cace6cc90014bd99739ba1b716749e0b6f93635f1b0692a2cdded95d9634df8d"
 assert d["dsr_source_compact_zip_sha256"]=="a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"
 assert d["ptde_source_fixed_zip_sha256"]=="6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281"
 assert "BND[id=200]" in d["dsr_source"] and "BD_M_9530_M.flver" in d["dsr_source"]
 slots=d["material_slots"]
 assert len(slots)==14
 assert all(s["slot"]==i and s["mesh"]==i for i,s in enumerate(slots))
 assert all(s["dsr_mtd"]==s["ptde_mtd"] for s in slots)
 assert all(s["spec_logical_both"] for s in slots)
 assert all(slots[i]["spec_logical_both"]=="BD_M_9530_2_s.tga" for i in (4,6,9))
 assert all(slots[i]["runtime_specrgb_id"]=="254fbaf3a67ccd46" for i in (4,6,9))
 assert slots[4]["dsr_mtd"]=="P_Leather[DSB].mtd"
 assert slots[9]["dsr_mtd"]=="P_DullLeather[DSB].mtd"
 assert slots[6]["dsr_mtd"]=="P_Metal[DSB].mtd"
 original=json.loads(ROUTER.read_text(encoding="utf-8"))["records"]
 hashes={
   "P_Leather[DSB].mtd":("0f2b9a1012b83c4d",1,"0x3fc00000","0x40800000"),
   "P_DullLeather[DSB].mtd":("7ea7d6ce9bb7148d",0,"0x3f800000","0x40000000"),
   "P_Metal[DSB].mtd":("ece70f36bd2517d2",2,"0x40200000","0x41080000"),
 }
 for name,(sha,slot,c101,c102) in hashes.items():
  matches=[r for r in original if r["mtd_name"]==name and r["dsr_mtd_sha256"].startswith(sha)]
  assert len(matches)==1
  r=matches[0]
  assert r["envspc_slot"]==slot and r["c101_f32_bits"]==[c101]*3 and r["c102_f32_bits"]==c102
 comparators=d["comparisons"]
 assert comparators[0]["a"]==4 and comparators[0]["b"]==9
 assert comparators[0]["mt_family"]=="NONMETAL_R8"
 assert comparators[1]["a"]==4 and comparators[1]["b"]==6
 assert comparators[1]["mt_family"]=="CROSS_OPERATOR_R8_VS_R19"
 assert all(c["matched_lerp_new_log"]==14 and c["matched_lerp_prior_log"]==12 for c in comparators)
 assert d["positive_wanderer_skin_submesh"].endswith("OPEN")
 assert d["causal_pixel_operator"]=="OPEN" and d["safe_patch_claim"] is False
 print("V331 SOURCE CONTRACT PASS: exact 9530_M full-manifest SHA, 14 slot/mesh/MTD/PTDE logical texture rows, shared SpecRGB 4/9/6 and authored EnvSpec 1/0/2. Runtime pixel OPEN.")
if __name__=="__main__":main()
