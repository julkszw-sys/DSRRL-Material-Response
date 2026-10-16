#!/usr/bin/env python3
"""Read-only original FLVER2 9530 mesh/bone palette audit.
Requires SHA-pinned private DSR_FLVER_CENSUS_COMPACT.zip, not game installation.
No vertex positions/UV or pixel behavior are inferred from structural bytes.
"""
from __future__ import annotations
import argparse, hashlib, json, struct, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WITNESS = ROOT/"data/census/v332_wanderer_9530_mesh_bone_operator_witness_v1.json"
EXPECTED = "a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"

def file_sha(p):
 h=hashlib.sha256()
 with p.open("rb") as f:
  for part in iter(lambda:f.read(1024*1024),b""):h.update(part)
 return h.hexdigest()

def parse(b):
 if b[:8]!=b"FLVER\0L\0":raise ValueError("unexpected FLVER")
 u=lambda o:struct.unpack_from("<I",b,o)[0]
 if u(8)!=0x2000c:raise ValueError("unknown structural layout")
 d,mc,n_b,n_m=(u(x) for x in (0x14,0x18,0x1c,0x20))
 bone_off=0x80+d*0x40+mc*0x20
 mesh_off=bone_off+n_b*0x80
 if mesh_off+n_m*0x30>len(b):raise ValueError("truncated structural mesh")
 def name(idx):
  off=u(bone_off+idx*0x80+0xc)
  if off<1 or off>=len(b):raise ValueError("invalid bone string offset")
  end=off
  while end+1<len(b) and b[end:end+2]!=b"\0\0":end+=2
  return b[off:end].decode("utf-16-le")
 bones=[name(i) for i in range(n_b)]
 out=[]
 for i in range(n_m):
  p=mesh_off+i*0x30
  slot=u(p+4);size=u(p+0x14);off=u(p+0x1c)
  if slot>=mc or size<1 or size>n_b or off+size*4>len(b):raise ValueError("invalid bone table")
  indices=struct.unpack_from("<"+str(size)+"i",b,off)
  if any(j<0 or j>=n_b for j in indices):raise ValueError("invalid bone index")
  out.append((slot,[bones[j] for j in indices]))
 return out

def main():
 a=argparse.ArgumentParser()
 a.add_argument("--source-zip",type=Path,help="Exact private DSR compact source archive")
 zargs=a.parse_args()
 d=json.loads(WITNESS.read_text(encoding="utf8"))
 runtime=d["runtime"]
 assert runtime["manifest_full_flver_sha256"].startswith("1398eec9e1545fb5")
 assert runtime["torso"]["mesh"]==runtime["torso"]["slot"]==4
 assert runtime["sleeves"]["mesh"]==runtime["sleeves"]["slot"]==9
 assert runtime["torso"]["mtd"]=="P_Leather[DSB].mtd"
 assert runtime["sleeves"]["mtd"]=="P_DullLeather[DSB].mtd"
 assert runtime["torso"]["envspec_slot"]==1 and runtime["sleeves"]["envspec_slot"]==0
 assert runtime["same_logical_specular"]=="BD_M_9530_2_s.tga"
 assert all("Fore" in v or "Arm" in v for v in runtime["sleeves"]["bone_names"])
 assert "Spine" in runtime["torso"]["bone_names"]
 if zargs.source_zip:
  if file_sha(zargs.source_zip)!=EXPECTED:raise ValueError("SHA of source archive mismatch")
  with zipfile.ZipFile(zargs.source_zip) as z:
   entries=json.loads(z.read("manifest.json"))
   for variant in ["BD_M_9530_M.flver","BD_M_9530.flver","BD_F_9530_M.flver","BD_F_9530.flver"]:
    hits=[r for r in entries if r["output"].endswith("_"+variant+".struct")]
    if len(hits)!=1:raise ValueError("missing original model variant")
    raw=z.read(hits[0]["output"])
    if hashlib.sha256(raw).hexdigest()!=hits[0]["sha256_struct"]:raise ValueError("invalid original struct digest")
    parsed=parse(raw)
    if variant=="BD_M_9530_M.flver":
     assert hits[0]["sha256_full"]==runtime["manifest_full_flver_sha256"]
     assert parsed[4]==(4,runtime["torso"]["bone_names"])
     assert parsed[9]==(9,runtime["sleeves"]["bone_names"])
    else:
     expected=next(v for v in d["variants"] if v["name"]==variant)
     torso=parsed[expected["torso_mesh"]][1]
     arms=parsed[expected["arms_only_mesh"]][1]
     assert "Spine" in torso and all("Fore" in n or "Arm" in n for n in arms)
  print("PASS: source ZIP full structural SHA + all four 9530 variant bone-palettes")
 else:print("PASS: witness schema and static source assertions only (source ZIP not provided)")
 print("VISUAL_MESH_OWNER_OPEN; GPU_PIXEL_OPEN; NO_ADDON_CHANGE")
if __name__=="__main__":main()
