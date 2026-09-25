#!/usr/bin/env python3
"""Rebuild the PTDE<->DSR FLVER pairwise material gate.

No SoulsFormats/Witchy/Yabber dependencies. The two external census ZIPs are
content-addressed inputs. The output is intentionally a small negative
refinement table for Diffuse/Normal: runtime still requires the existing exact
positive PTDE capability and exact DSR raw-MTD identity. Positive runtime activation remains fail-open until an exact DSR (FLVER identity, material slot, MTD) tuple corpus is materialized.
"""
from __future__ import annotations
import argparse, collections, hashlib, json, struct, zipfile
from pathlib import Path

DSR_SHA="a9a2e0eb48625fc735dfe18f7f375105cc5f56be005022969300c778489d0891"
PTDE_SHA="6651acb4fe995a69f1c84e5761b3d6bdcadeb39e1fc4d1087c3b82794f4d7281"
BITS={"g_Diffuse":1,"g_Bumpmap":2,"g_DetailBumpmap":4,"g_Specular":8,
      "g_Lightmap":16,"g_Diffuse_2":32,"g_Bumpmap_2":64,"g_Specular_2":128}
EXPECTED={"dsr_materials":19985,"ptde_materials":21315,"dsr_mtds":367,
          "ptde_mtds":261,"overlap":256,"diffuse_reject":6,"bump_reject":6}

def sha(path:Path)->str:return hashlib.sha256(path.read_bytes()).hexdigest()
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
        vals=[];ok=True
        for j in range(n):
            po,so=struct.unpack_from("<II",b,s+j*0x20)
            if so+4>len(b) or b[so:so+4]!=b"g\x00_\x00" or (po and po>=len(b)):
                ok=False;break
            vals.append((u16(b,po),u16(b,so,128)))
        if ok:return vals
    raise ValueError("FLVER texture table not found")

def dsr_materials(path:Path):
    out={}
    with zipfile.ZipFile(path) as z:
        manifest=json.loads(z.read("manifest.json"));names=set(z.namelist());seen=set()
        for r in manifest:
            member=r["output"]
            if member in seen or member not in names:continue
            seen.add(member);b=z.read(member)
            if b[:8]!=b"FLVER\0L\0":continue
            dummy=u32(b,0x14);mc=u32(b,0x18);tc=u32(b,0x58)
            tv=tex_table(b,tc);ms=0x80+dummy*0x40
            for i in range(mc):
                no,mo,n,idx=struct.unpack_from("<IIII",b,ms+i*0x20)
                name=base(u16(b,mo));k=(r["sha256_full"],i)
                present=resource=0
                for j in range(n):
                    p,sem=tv[idx+j];bit=BITS.get(sem,0)
                    present|=bit
                    if p:resource|=bit
                out[k]=(name,present,resource)
    return out

def ptde_materials(path:Path):
    out={}
    with zipfile.ZipFile(path) as z:
        member=next(n for n in z.namelist() if n.endswith("flver_material_ownership_raw_v1.jsonl"))
        for line in z.read(member).decode("utf-8").splitlines():
            if not line:continue
            r=json.loads(line);k=(r["flver_sha256"],int(r["material_slot"]))
            x=out.setdefault(k,[base(r["mtd_name"]),0,0])
            bit=BITS.get(r["texture_semantic"],0);x[1]|=bit
            if r.get("texture_path"):x[2]|=bit
    return {k:tuple(v) for k,v in out.items()}

def aggregate(materials):
    g=collections.defaultdict(list);case={}
    for name,present,resource in materials.values():
        low=name.lower();g[low].append((present,resource));case.setdefault(low,name)
    out={}
    for low,vals in g.items():
        pu=ru=0;pi=ri=0xff
        for p,r in vals:pu|=p;ru|=r;pi&=p;ri&=r
        out[low]={"count":len(vals),"present_union":pu,"present_intersection":pi,
                  "resource_union":ru,"resource_intersection":ri,"name":case[low]}
    return out

def rejects(d,p,bit):
    result=[]
    for low,x in p.items():
        if not (x["present_union"]&bit):continue
        if low not in d or not (d[low]["resource_intersection"]&x["resource_intersection"]&bit):
            result.append((x["name"],fnv(x["name"])))
    return sorted(result,key=lambda x:x[0].lower())

def render_header(diff,bump):
    def arr(name,rows):
        lines=[f"inline constexpr std::array<std::uint64_t,{len(rows)}> {name} = {{{{"]
        for n,h in rows:lines.append(f"    0x{h:016x}ull, // {n}")
        lines.append("}};");return "\n".join(lines)
    return f"""#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace dsrrl::operators::material_response::generated {{
inline constexpr char k_flver_pairwise_dsr_zip_sha256[]="{DSR_SHA}";
inline constexpr char k_flver_pairwise_ptde_zip_sha256[]="{PTDE_SHA}";
inline constexpr std::size_t k_flver_pairwise_dsr_material_count=19985u;
inline constexpr std::size_t k_flver_pairwise_ptde_material_count=21315u;
inline constexpr std::size_t k_flver_pairwise_dsr_mtd_count=367u;
inline constexpr std::size_t k_flver_pairwise_ptde_mtd_count=261u;
inline constexpr std::size_t k_flver_pairwise_overlap_mtd_count=256u;
inline constexpr bool k_flver_pairwise_owner_tuple_authentication_available=false;
constexpr bool flver_pairwise_owner_tuple_authenticated(std::uint64_t,std::uint32_t,std::uint64_t) noexcept{{return false;}}
{arr("k_flver_pairwise_diffuse_reject",diff)}
{arr("k_flver_pairwise_bump_reject",bump)}
template<std::size_t N> constexpr bool pairwise_rejected(std::uint64_t h,const std::array<std::uint64_t,N>&a) noexcept{{if(!h)return true;for(auto x:a)if(x==h)return true;return false;}}
constexpr bool flver_pairwise_diffuse_stable_after_ptde_positive(std::uint64_t h) noexcept{{return !pairwise_rejected(h,k_flver_pairwise_diffuse_reject);}}
constexpr bool flver_pairwise_bump_stable_after_ptde_positive(std::uint64_t h) noexcept{{return !pairwise_rejected(h,k_flver_pairwise_bump_reject);}}
}} // namespace dsrrl::operators::material_response::generated
"""

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--dsr-zip",required=True,type=Path);ap.add_argument("--ptde-zip",required=True,type=Path);ap.add_argument("--header",type=Path);ap.add_argument("--report",type=Path)
    a=ap.parse_args()
    if sha(a.dsr_zip)!=DSR_SHA:raise SystemExit("DSR census SHA mismatch")
    if sha(a.ptde_zip)!=PTDE_SHA:raise SystemExit("PTDE census SHA mismatch")
    dm=dsr_materials(a.dsr_zip);pm=ptde_materials(a.ptde_zip);d=aggregate(dm);p=aggregate(pm)
    overlap=set(d)&set(p);diff=rejects(d,p,1);bump=rejects(d,p,2)
    got={"dsr_materials":len(dm),"ptde_materials":len(pm),"dsr_mtds":len(d),"ptde_mtds":len(p),"overlap":len(overlap),"diffuse_reject":len(diff),"bump_reject":len(bump)}
    if got!=EXPECTED:raise SystemExit(f"census invariant mismatch: {got}")
    if a.header:a.header.write_text(render_header(diff,bump),encoding="utf-8")
    report={"schema":"dsrrl.flver_pairwise_material_census.v1","inputs":{"dsr_sha256":DSR_SHA,"ptde_sha256":PTDE_SHA},"counts":got,
            "stable_shared":{"diffuse":sum(bool(d[k]["resource_intersection"]&p[k]["resource_intersection"]&1) for k in overlap),
                             "bump":sum(bool(d[k]["resource_intersection"]&p[k]["resource_intersection"]&2) for k in overlap),
                             "specular":sum(bool(d[k]["resource_intersection"]&p[k]["resource_intersection"]&8) for k in overlap)},
            "diffuse_reject":[{"mtd":n,"hash":f"0x{h:016x}"} for n,h in diff],
            "bump_reject":[{"mtd":n,"hash":f"0x{h:016x}"} for n,h in bump]}
    if a.report:a.report.write_text(json.dumps(report,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(json.dumps(report,ensure_ascii=False))
if __name__=="__main__":main()
