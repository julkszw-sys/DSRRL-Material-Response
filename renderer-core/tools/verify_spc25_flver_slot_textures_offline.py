#!/usr/bin/env python3
"""Read-only DS1R FLVER2 material->g_Specular source identity audit.

Input: owner-supplied DSR parts evidence ZIP and canonical 25-MTD roster CSV.
Verifies full FLVER2 material/texture pointer tables. Does NOT prove native SRV t1.
"""
import argparse,csv,hashlib,json,struct,zipfile,zlib
from collections import defaultdict
from pathlib import Path

ZIP_SHA="a76b13c909fb4a30a1a312cba7d594a851193bef55e578934829fd873f872c97"
def U(b,i): return struct.unpack_from("<I",b,i)[0]
def S(b,i,wide):
    if i>=len(b): raise ValueError("FLVER string pointer outside file")
    if wide:
        end=i
        while end+1<len(b) and end-i<1024 and b[end:end+2]!=b"\0\0": end+=2
        if end+1>=len(b) or end-i>=1024: raise ValueError("bad UTF16 terminator")
        return b[i:end].decode("utf-16le")
    end=b.find(b"\0",i,min(i+1024,len(b)))
    if end<0: raise ValueError("bad ANSI terminator")
    return b[i:end].decode("ascii")
def parse(data,source):
    if data[:8]!=b"FLVER\0L\0":raise ValueError("not DS1R little-endian FLVER")
    if not 0x20000<=U(data,8)<0x30000:raise ValueError("not FLVER2")
    limit=U(data,12)+U(data,16)
    if limit>len(data) or limit<0x80:raise ValueError("bad FLVER length")
    b=data[:limit]
    dummy,material,bones,meshes,vb=struct.unpack_from("<5I",b,0x14)
    nt=U(b,0x58);wide=bool(b[0x49])
    if max(dummy,material,bones,meshes,vb,nt)>10000:raise ValueError("bad counts")
    start=0x80+dummy*64;end=start+material*32
    if end>len(b):raise ValueError("material table out of bounds")
    materials=[]
    for i in range(material):
        a,t,count,first,flags,gx,x,y=struct.unpack_from("<8I",b,start+i*32)
        if first+count>nt or x or y:raise ValueError("material texture index invalid")
        m=S(b,t,wide).replace("\\","/").split("/")[-1]
        if not m.lower().endswith(".mtd"):raise ValueError("not an MTD pointer")
        materials.append((i,m,count,first))
    tex=[]
    if nt:
        candidates=[]
        for p in range(end,U(b,12)-nt*32+1,4):
            trial=[]
            for j in range(nt):
                a,t=struct.unpack_from("<II",b,p+j*32)
                if min(a,t)<p+nt*32 or max(a,t)>=len(b):break
                try: path,kind=S(b,a,wide),S(b,t,wide)
                except (UnicodeError,ValueError):break
                if not(kind.startswith("g_") and 2<=len(kind)<=48 and
                       (not path or path.lower().endswith(".tga"))):break
                trial.append((kind,path.replace("\\","/").split("/")[-1].rsplit(".",1)[0] if path else ""))
            if len(trial)==nt:candidates.append(trial)
        if len(candidates)!=1:raise ValueError("texture table not structurally unique")
        tex=candidates[0]
    else:
        if any(count for _,_,count,_ in materials):raise ValueError("empty texture table")
    digest=hashlib.sha256(b).hexdigest()
    out=[]
    for slot,mtd,count,first in materials:
        t=tex[first:first+count]
        out.append(dict(source=source,flver_sha256=digest,material_slot=slot,mtd=mtd,
           g_specular=[name for kind,name in t if kind=="g_Specular" and name],
           g_diffuse=[name for kind,name in t if kind=="g_Diffuse" and name],
           g_bumpmap=[name for kind,name in t if kind=="g_Bumpmap" and name]))
    return out,nt

def run(archive,roster):
    sha=hashlib.sha256(Path(archive).read_bytes()).hexdigest()
    if sha!=ZIP_SHA:raise ValueError("owner evidence ZIP SHA mismatch "+sha)
    with open(roster,encoding="utf-8-sig",newline="") as f:
        profiles={r["mtd_name"]:r for r in csv.DictReader(f)}
    if len(profiles)!=25:raise ValueError("expected exactly 25 canonical SPC MTDs")
    rows=[];files=models=withtex=0
    with zipfile.ZipFile(archive) as z:
        for name in z.namelist():
            if not(name.startswith("DSR/parts/") and name.endswith(".partsbnd.dcx")):continue
            files+=1;b=z.read(name)
            if b[:4]!=b"DCX\0" or b[0x24:0x2c]!=b"DCP\0DFLT":
                raise ValueError("unexpected DCX "+name)
            decoded=zlib.decompress(b[0x4c:])
            if len(decoded)!=struct.unpack_from(">I",b,0x1c)[0]:
                raise ValueError("bad DCX size")
            i=0
            while True:
                i=decoded.find(b"FLVER\0L\0",i)
                if i<0:break
                current,ntex=parse(decoded[i:],name)
                models+=1;withtex+=bool(ntex);rows.extend(current);i+=6
    exact=[dict(r,route_index=int(profiles[r["mtd"]]["route_index"])) for r in rows
           if r["mtd"] in profiles]
    byname=defaultdict(list)
    for r in rows:
        for spec in r["g_specular"]:
            byname[spec.casefold()].append([r["source"],r["material_slot"],r["mtd"]])
    cross={name:values for name,values in byname.items()
           if len({x[2] for x in values})>1}
    seen={r["mtd"] for r in exact}
    return dict(source_proof="EXACT_FLVER_MATERIAL_TO_TEXTURE_ONLY",
      native_d3d11_srv_identity="OPEN",ptde_pixels="OPEN",
      archive_sha256=sha,partsbnds=files,flver2_models=models,
      flver_with_textures=withtex,flver_no_textures=models-withtex,
      material_slots=len(rows),spc25_matching_slots=len(exact),
      spc25_profiles_covered=sorted(seen),
      spc25_profiles_not_present=sorted(set(profiles)-seen),
      cross_mtd_specular_aliases=cross,exact_spc_records=exact)

if __name__=="__main__":
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument("dsr_parts_zip");a.add_argument("spc25_exact_roster_csv")
    a.add_argument("--out",required=True)
    v=a.parse_args()
    result=run(v.dsr_parts_zip,v.spc25_exact_roster_csv)
    Path(v.out).write_text(json.dumps(result,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
    print(json.dumps({k:value for k,value in result.items()
                      if k not in ("exact_spc_records","cross_mtd_specular_aliases")},
                     indent=2,ensure_ascii=False))
