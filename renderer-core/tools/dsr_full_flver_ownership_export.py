#!/usr/bin/env python3
"""DSR source-complete FLVER/material ownership exporter.

Read-only scanner for a Dark Souls Remastered extracted asset root. It deliberately
emits ownership facts only; it does not infer PTDE equivalence. The output schema
matches flver_material_ownership_census.py so PTDE and DSR corpora can be joined
only after both sides are source-complete.

Input root is expected to contain extracted FLVER/FLVER2 and MTD files. Unknown
or unresolved material references are emitted separately and cannot be mistaken
for operator absence.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,struct
from collections import defaultdict
from pathlib import Path
SCHEMA=("game","flver_identity","material_slot","mtd_name","mtd_sha256","spx_sha256","receiver_index","consumer_family","material_family","texture_semantic","resource_hash","srv_slot","sampler_slot")
def sha(b):return hashlib.sha256(b).hexdigest()
def cstr(b,o):
    if o<0 or o>=len(b):return ""
    e=b.find(b"\0",o);e=len(b) if e<0 else e
    for enc in ("utf-8","shift_jis","cp932","latin1"):
        try:return b[o:e].decode(enc)
        except UnicodeDecodeError:pass
    return b[o:e].decode("latin1",errors="replace")
def material_records(data):
    if not data.startswith(b"FLVER\0") or len(data)<0x80:return []
    endian=">" if data[6:8]==b"B\0" else "<"
    def i32(o):return struct.unpack_from(endian+"i",data,o)[0]
    candidates=[]
    for co,to in ((0x20,0x24),(0x24,0x28),(0x28,0x2c)):
        try:n=i32(co);off=i32(to)
        except struct.error:continue
        if 0<n<10000 and 0x40<=off<len(data):candidates.append((n,off))
    for n,off in candidates:
        for stride in (0x20,0x1c,0x18):
            if off+n*stride>len(data):continue
            rows=[];ok=True
            for k in range(n):
                p=off+k*stride
                try:no=i32(p);mo=i32(p+4)
                except struct.error:ok=False;break
                name=cstr(data,no);mtd=cstr(data,mo)
                if not mtd.lower().endswith(".mtd"):ok=False;break
                rows.append((k,name,mtd))
            if ok:return rows
    return []
def main():
    ap=argparse.ArgumentParser();ap.add_argument("root",type=Path);ap.add_argument("--out",type=Path,default=Path("dsr_flver_ownership.tsv"));ap.add_argument("--unresolved",type=Path,default=Path("dsr_flver_ownership_unresolved.tsv"));a=ap.parse_args();mtd=defaultdict(list)
    for p in a.root.rglob("*"):
        if p.is_file() and p.suffix.lower()==".mtd":mtd[p.name.lower()].append((p,sha(p.read_bytes())))
    rows=[];bad=[];flvers=0
    for p in a.root.rglob("*"):
        if not p.is_file():continue
        try:b=p.read_bytes()
        except OSError:continue
        if not b.startswith(b"FLVER\0"):continue
        flvers+=1;ident=f"{p.relative_to(a.root).as_posix()}#{sha(b)}";recs=material_records(b)
        if not recs:bad.append((ident,"FLVER material table unresolved"));continue
        for slot,_,name in recs:
            hits=mtd.get(Path(name.replace('\\','/')).name.lower(),[])
            if len(hits)!=1:bad.append((ident,f"slot={slot};mtd={name};matches={len(hits)}"));continue
            _,mh=hits[0];rows.append(("DSR",ident,slot,name,mh,"","","","","","","",""))
    with a.out.open("w",newline="",encoding="utf-8") as f:w=csv.writer(f,delimiter="\t",lineterminator="\n");w.writerow(SCHEMA);w.writerows(rows)
    with a.unresolved.open("w",newline="",encoding="utf-8") as f:w=csv.writer(f,delimiter="\t",lineterminator="\n");w.writerow(("flver_identity","reason"));w.writerows(bad)
    s={"game":"DSR","flver_files":flvers,"resolved_material_rows":len(rows),"unresolved_rows":len(bad),"source_complete":len(bad)==0 and flvers>0,"policy":"absence is not operator absence unless source_complete=true"};print(json.dumps(s,indent=2,sort_keys=True));return 0 if s["source_complete"] else 2
if __name__=="__main__":raise SystemExit(main())
