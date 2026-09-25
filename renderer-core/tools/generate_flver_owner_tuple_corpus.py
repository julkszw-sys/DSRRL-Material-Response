#!/usr/bin/env python3
"""Materialize the exact DSR (FLVER digest, material slot, MTD) owner corpus.

PTDE annotations are MTD-level capability metadata only. They never prove
cross-version FLVER homology or pixel equivalence. Runtime activation must
match the actual DSR FLVER SHA-256 + slot + exact MTD identity and fail open
otherwise.
"""
from __future__ import annotations
import argparse, collections, hashlib, io, json, struct, zipfile
from pathlib import Path

DSR_SHA="a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"
PTDE_SHA="6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281"
BITS={"g_Diffuse":1,"g_Bumpmap":2,"g_DetailBumpmap":4,"g_Specular":8,
      "g_Lightmap":16,"g_Diffuse_2":32,"g_Bumpmap_2":64,"g_Specular_2":128}

def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def u32(b:bytes,o:int)->int:return struct.unpack_from("<I",b,o)[0]
def u16(b:bytes,o:int,limit:int=512)->str:
    if o<0 or o>=len(b):return ""
    e=min(len(b),o+limit*2);p=o
    while p+1<e and b[p:p+2]!=b"\0\0":p+=2
    return b[o:p].decode("utf-16le",errors="replace")
def base(p:str)->str:return p.replace("\\","/").rsplit("/",1)[-1]
def fnv(text:str)->int:
    h=14695981039346656037
    for b in text.encode("utf-8"):h=((h^b)*1099511628211)&0xffffffffffffffff
    return h
def tex_table(b:bytes,n:int):
    if not n:return []
    for s in range(0x80,len(b)-n*0x20+1,4):
        vals=[]
        for j in range(n):
            po,so=struct.unpack_from("<II",b,s+j*0x20)
            if so+4>len(b) or b[so:so+4]!=b"g\x00_\x00" or (po and po>=len(b)):break
            vals.append((u16(b,po),u16(b,so,128)))
        else:return vals
    raise ValueError("FLVER texture table not found")

def ptde_mtd_caps(path:Path):
    caps=collections.defaultdict(lambda:[0,0,0]); owners=set()
    with zipfile.ZipFile(path) as z:
        member=next(n for n in z.namelist() if n.endswith("flver_material_ownership_raw_v1.jsonl"))
        for line in io.TextIOWrapper(z.open(member),encoding="utf-8"):
            r=json.loads(line); low=base(r["mtd_name"]).lower()
            bit=BITS.get(r["texture_semantic"],0); caps[low][1]|=bit
            if r.get("texture_path"):caps[low][2]|=bit
            owners.add((low,r["flver_sha256"],int(r["material_slot"])))
    for low,_,_ in owners:caps[low][0]+=1
    return caps

def dsr_rows(path:Path,caps):
    rows=[]
    with zipfile.ZipFile(path) as z:
        manifest=json.loads(z.read("manifest.json")); names=set(z.namelist()); seen=set()
        for r in manifest:
            member=r["output"]
            if member in seen or member not in names:continue
            seen.add(member); b=z.read(member)
            if b[:8]!=b"FLVER\0L\0":continue
            dummy=u32(b,0x14);mc=u32(b,0x18);tc=u32(b,0x58)
            tv=tex_table(b,tc);ms=0x80+dummy*0x40
            for slot in range(mc):
                _,mo,n,idx=struct.unpack_from("<IIII",b,ms+slot*0x20)
                mtd=base(u16(b,mo)); present=resource=0
                for j in range(n):
                    p,sem=tv[idx+j];bit=BITS.get(sem,0);present|=bit
                    if p:resource|=bit
                p=caps.get(mtd.lower())
                rows.append({"flver_sha256":r["sha256_full"],"material_slot":slot,
                    "mtd_name":mtd,"mtd_name_fnv1a64":f"{fnv(mtd):016x}",
                    "dsr_present_mask":present,"dsr_resource_mask":resource,
                    "ptde_mtd_present":bool(p),"ptde_owner_count":p[0] if p else 0,
                    "ptde_present_union":p[1] if p else 0,
                    "ptde_resource_union":p[2] if p else 0})
    rows.sort(key=lambda x:(x["flver_sha256"],x["material_slot"],x["mtd_name"].lower()))
    return rows

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--dsr-zip",required=True,type=Path)
    ap.add_argument("--ptde-zip",required=True,type=Path);ap.add_argument("--out",required=True,type=Path)
    ap.add_argument("--report",required=True,type=Path);a=ap.parse_args()
    if sha(a.dsr_zip)!=DSR_SHA:raise SystemExit("DSR census SHA mismatch")
    if sha(a.ptde_zip)!=PTDE_SHA:raise SystemExit("PTDE census SHA mismatch")
    rows=dsr_rows(a.dsr_zip,ptde_mtd_caps(a.ptde_zip))
    if len(rows)!=19985:raise SystemExit(f"DSR material invariant mismatch: {len(rows)}")
    keys={(x["flver_sha256"],x["material_slot"],x["mtd_name"].lower()) for x in rows}
    if len(keys)!=len(rows):raise SystemExit("owner tuple collision/duplicate")
    with a.out.open("w",encoding="utf-8") as f:
        for x in rows:f.write(json.dumps(x,separators=(",",":"),ensure_ascii=False)+"\n")
    report={"schema":"dsrrl.dsr_flver_owner_tuple_corpus.v1","rows":len(rows),
      "unique_tuples":len(keys),"duplicate_tuples":0,
      "ptde_mtd_present_rows":sum(x["ptde_mtd_present"] for x in rows),
      "ptde_mtd_absent_rows":sum(not x["ptde_mtd_present"] for x in rows),
      "corpus_sha256":sha(a.out),"dsr_zip_sha256":DSR_SHA,"ptde_zip_sha256":PTDE_SHA,
      "activation_policy":"Exact DSR tuple is an authentication prerequisite only. PTDE MTD capability does not prove cross-version FLVER homology; unknown identity/consumer fails open."}
    a.report.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(report,indent=2))
if __name__=="__main__":main()
