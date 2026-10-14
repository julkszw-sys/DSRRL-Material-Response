#!/usr/bin/env python3
"""V304 complete owner-original DSR DX11 FlverPBL HemEnvLerp census.
Reads the user's licensed ZIP; no original shader bytes are committed.
Source-only; matching ABI is not proof of PTDE operator/pixel equivalence.
"""
import argparse,collections,hashlib,json,struct,zipfile,zlib
from pathlib import Path
BINDER="DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"
DCX_SHA="ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394"
BND_SHA="9ad94df9e282e6fd559605506aa16d74eeb1ef3ebb1ed3002c5affee80018464"
BASE={
"FRPG_Phn_DifSpcBmp______Csd_HemEnvLerp.fpo":"7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",
"FRPG_Phn_DifSpcBmp______Sdw_HemEnvLerp.fpo":"857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",
"FRPG_Phn_DifSpcBmp__________HemEnvLerp.fpo":"9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8"
}
def u32(b,off):
 if off<0 or off+4>len(b):raise ValueError("invalid offset")
 return struct.unpack_from("<I",b,off)[0]
def sha(b):return hashlib.sha256(b).hexdigest()
def chunks(raw):
 if raw[:4]!=b"DXBC" or len(raw)<64 or u32(raw,24)!=len(raw):
  raise ValueError("invalid DXBC")
 count=u32(raw,28)
 if not 2<=count<=32 or 32+count*4>len(raw):raise ValueError("invalid chunks")
 out={}
 for i in range(count):
  at=u32(raw,32+4*i)
  size=u32(raw,at+4)
  if at+8+size>len(raw):raise ValueError("invalid chunk size")
  tag=raw[at:at+4]
  if tag in out:raise ValueError("repeated chunk")
  out[tag]=raw[at+8:at+8+size]
 return out
def instructions(raw):
 c=chunks(raw)
 if b"SHEX" not in c or len(c[b"SHEX"])%4:raise ValueError("missing SHEX")
 w=struct.unpack("<"+"I"*(len(c[b"SHEX"])//4),c[b"SHEX"])
 if w[1]!=len(w):raise ValueError("wrong word length")
 at=2
 while at<len(w):
  length=(w[at]>>24)&127
  if length==0 or at+length>len(w):raise ValueError("invalid instruction")
  yield w[at:at+length]
  at+=length
def main(archive):
 with zipfile.ZipFile(archive) as z:
  original=z.read(BINDER)
  ptde={Path(n).name for n in z.namelist()
        if "/PTDE_" in n and n.endswith(".fpo")}
 if sha(original)!=DCX_SHA or original[:4]!=b"DCX\x00":
  raise ValueError("owner original DCX SHA mismatch")
 raw=zlib.decompress(original[76:])
 if sha(raw)!=BND_SHA or raw[:4]!=b"BND3":raise ValueError("BND original SHA mismatch")
 count=u32(raw,16)
 if count!=1989:raise ValueError("unexpected DX11 shader count")
 rows=[]
 for idx in range(count):
  pos=32+idx*24
  flags,size,off,rid,noff,other=struct.unpack_from("<6I",raw,pos)
  if off+size>len(raw) or rid!=idx or not 0<noff<len(raw):
   raise ValueError("BND entry invalid")
  name=raw[noff:raw.index(b"\x00",noff)].decode("ascii")
  data=raw[off:off+size]
  meta=chunks(data)
  if name not in BASE and "HemEnvLerp" not in name:continue
  entry={"id":idx,"name":name,"sha256":sha(data),
         "ptde_name_exists":name in ptde}
  if "HemEnvLerp" in name and name.startswith("FRPG_Phn_"):
   inst=list(instructions(data))
   s7=[x for x in inst if (x[0]&2047)==0x5a and len(x)==3 and x[2]==7]
   if len(s7)>1:raise ValueError("duplicate s7 declarations")
   entry["s7"]=("none" if not s7 else
     "comparison" if s7[0][0]==0x0300085a else
     "regular" if s7[0][0]==0x0300005a else "unknown")
   if entry["s7"]=="unknown":raise ValueError("unrecognized s7")
   entry["outputs"]=u32(meta[b"OSGN"],0)
   entry["inputs"]=u32(meta[b"ISGN"],0)
   entry["isgn_sha"]=sha(meta[b"ISGN"])
   entry["osgn_sha"]=sha(meta[b"OSGN"])
   entry["rdef_sha"]=sha(meta[b"RDEF"])
   entry["sao_full_surface"]=sum(
      (x[0]&2047)==0x48 and len(x)==13 and x[8]==8 and x[10]==8
      and (inst[j+1][0]&2047)==0x38
      and len(inst[j+1])==7 and inst[j+1][3]==0x00100e56
      for j,x in enumerate(inst[:-1]))
  rows.append(entry)
 phn=[r for r in rows if r["name"].startswith("FRPG_Phn_") and "HemEnvLerp" in r["name"]]
 all_lerp=[r for r in rows if "HemEnvLerp" in r["name"]]
 bases=[next(r for r in phn if r["name"]==name) for name in BASE]
 for name,digest in BASE.items():
  if next(r for r in bases if r["name"]==name)["sha256"]!=digest:
   raise ValueError("exact material base SHA mismatch")
 def same_io(r):
  return any(r["isgn_sha"]==b["isgn_sha"] and
   r["osgn_sha"]==b["osgn_sha"] for b in bases)
 report={
  "scope":"Owner-original FlverPBL DX11 shaderbnd, static source only",
  "dxbc_total":count,"hem_env_lerp_total":len(all_lerp),
  "hem_env_lerp_family":dict(collections.Counter(
    r["name"].split("_")[1] for r in all_lerp)),
  "phn_hem_env_lerp":len(phn),
  "ptde_filename_overlap":sum(r["ptde_name_exists"] for r in all_lerp),
  "phn_matched_input_output_signature":sum(same_io(r) for r in phn),
  "phn_s7_comparison":sum(r["s7"]=="comparison" for r in phn),
  "phn_s7_regular":sum(r["s7"]=="regular" for r in phn),
  "phn_s7_absent":sum(r["s7"]=="none" for r in phn),
  "phn_s7_regular_with_two_outputs":sum(r["s7"]=="regular" and r["outputs"]==2 for r in phn),
  "phn_s7_regular_with_four_outputs":sum(r["s7"]=="regular" and r["outputs"]==4 for r in phn),
  "phn_s7_regular_same_io_as_base":sum(r["s7"]=="regular" and same_io(r) for r in phn),
  "base_exact":[{"id":r["id"],"name":r["name"],"sha256":r["sha256"],
                 "input_count":r["inputs"],"output_count":r["outputs"],
                 "s7":r["s7"],"sao":r["sao_full_surface"]}
                for r in bases],
  "same_base_io_candidates":[{"id":r["id"],"name":r["name"],"s7":r["s7"],
         "same_exact_rdef_as_any_base":any(r["rdef_sha"]==b["rdef_sha"] for b in bases)}
        for r in phn if same_io(r)],
  "limitations":"No pixel equivalence; alternative materials or multiple-point-light shader have different consumer/resource semantics. No blanket shader replacement."
 }
 if (report["hem_env_lerp_total"],report["phn_hem_env_lerp"],
     report["phn_s7_regular"],report["phn_s7_regular_with_four_outputs"],
     report["phn_s7_regular_same_io_as_base"])!=(672,336,48,48,0):
  raise AssertionError("shader census topology changed")
 return report
if __name__=="__main__":
 p=argparse.ArgumentParser()
 p.add_argument("owner_original_archive")
 p.add_argument("--report",type=Path)
 a=p.parse_args()
 result=main(a.owner_original_archive)
 if a.report:a.report.write_text(json.dumps(result,indent=2)+"\n")
 print("V304_FULL_DSR_DX11_HEMENVLERP_CENSUS_PASS",json.dumps(result))
