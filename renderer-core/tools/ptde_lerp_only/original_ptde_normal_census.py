#!/usr/bin/env python3
"""Original PTDE 24 no-PointLight shader Stable versus Lerp source-token census.

Run with owner's private DSRRL_FINAL_EVIDENCE_PART02 ZIP:
  python renderer-core/tools/ptde_lerp_only/original_ptde_normal_census.py \
    DSRRL_FINAL_EVIDENCE_PART02.zip --out ptde_24_census
Never redistribute original .fpo, do not interpret opcode similarity as GPU pixel PASS.
"""
from __future__ import annotations
import argparse,csv,hashlib,json,struct,zipfile,difflib
from collections import Counter
from pathlib import Path

def decode(blob:bytes):
    if len(blob)%4:raise ValueError("unaligned shader bytes")
    w=struct.unpack("<%dI"%(len(blob)//4),blob)
    if w[0]!=0xffff0300:raise ValueError("not PTDE ps_3_0")
    i=1;out=[]
    while i<len(w):
        op=w[i]&65535
        if op==65535:break
        if op==65534:
            i+=1+((w[i]>>16)&32767)
            continue
        n=(w[i]>>24)&15
        if not n or i+n>=len(w):raise ValueError("invalid shader token size")
        out.append((op,tuple(w[i:i+n+1])))
        i+=n+1
    return out

def check(a,b):
    left=[x for x in a if x[0] not in (31,81)]
    right=[x for x in b if x[0] not in (31,81)]
    p=next((i for i,x in enumerate(left) if x[0]==66),len(left))
    q=next((i for i,x in enumerate(right) if x[0]==66),len(right))
    match=difflib.SequenceMatcher(None,left,right,autojunk=False)
    matched=sum(x.size for x in match.get_matching_blocks())
    z=dict(stable_nondef=len(left),lerp_nondef=len(right),
           bit_exact_ordered_matches=matched,
           stable_match_fraction=round(matched/len(left),6),
           pre_first_texld_identical=left[:p]==right[:q])
    for op,name in ((36,"nrm"),(7,"rsq"),(9,"dp4"),(8,"dp3")):
        va=[x[1] for x in a if x[0]==op]
        vb=[x[1] for x in b if x[0]==op]
        z[name+"_count"]=len(va)
        z[name+"_tokens_identical"]=va==vb
    ca,cb=Counter(t for t,_ in a),Counter(t for t,_ in b)
    z["opcode_count_delta"]={str(o):cb[o]-ca[o] for o in sorted(ca.keys()|cb.keys()) if ca[o]!=cb[o]}
    return z

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("archive",type=Path)
    ap.add_argument("--out",type=Path,default=Path("ptde_24_normal_census"))
    x=ap.parse_args();rows=[]
    with zipfile.ZipFile(x.archive) as z:
        names={p.rsplit("/",1)[-1]:p for p in z.namelist()
               if "PTDE_FRPG_PHN_EXTRACTED" in p and p.endswith(".fpo")
               and "FRPG_Phn_DifSpc" in p and "Pnt" not in p}
        for lerp in sorted(k for k in names if k.endswith("HemEnvLerp.fpo")):
            stable=lerp.replace("HemEnvLerp.fpo","HemEnv.fpo")
            if stable not in names:raise ValueError(("missing exact stable shader",lerp))
            a,b=z.read(names[stable]),z.read(names[lerp])
            rows.append(dict(family=lerp.replace("HemEnvLerp.fpo",""),
                 stable_shader=stable,lerp_shader=lerp,
                 stable_sha256=hashlib.sha256(a).hexdigest(),
                 lerp_sha256=hashlib.sha256(b).hexdigest(),
                 **check(decode(a),decode(b))))
    assert len(rows)==24
    assert all(x["pre_first_texld_identical"] for x in rows)
    assert all(x["nrm_tokens_identical"] and x["rsq_tokens_identical"] and x["dp4_tokens_identical"] for x in rows)
    assert sum(x["nrm_count"] for x in rows)==132
    assert sum(x["rsq_count"] for x in rows)==108
    assert sum(x["dp4_count"] for x in rows)==96
    assert sum(x["bit_exact_ordered_matches"] for x in rows)==3396
    x.out.mkdir(parents=True,exist_ok=True)
    (x.out/"24_exact_pairs.json").write_text(json.dumps(rows,indent=2)+"\n")
    keys=("family stable_sha256 lerp_sha256 stable_nondef lerp_nondef "
         "bit_exact_ordered_matches stable_match_fraction pre_first_texld_identical "
         "nrm_count nrm_tokens_identical rsq_count rsq_tokens_identical "
         "dp4_count dp4_tokens_identical dp3_count dp3_tokens_identical").split()
    with (x.out/"24_exact_pairs.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=keys,extrasaction="ignore");w.writeheader();w.writerows(rows)
    print("PTDE_ORIGINAL_24_SHADER_PAIRS_SOURCE_PASS; NRM=132 RSQ=108 DP4=96 EXACT")
    print("GPU_DSR_LERP_NORMAL_LIVEOUT=OPEN; PTDE_PIXEL=OPEN")
if __name__=="__main__":main()
