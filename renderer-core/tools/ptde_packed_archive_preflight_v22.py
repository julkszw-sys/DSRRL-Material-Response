#!/usr/bin/env python3
"""Packed PTDE SOURCE-LAYOUT preflight only. Two 2012 retail archive pairs
are a supported input layout. This does not certify full armor texture coverage.
No PTDE unpack, no game/addon file modification.
"""
import argparse
import json
import os
from pathlib import Path
import re
import sys

rx=re.compile(r"^dvdbnd([0-3])\\.(bhd5|bdt)$",re.I)

def inspect(root: Path):
    root=root.resolve()
    if not root.is_dir(): raise ValueError(f"Missing PTDE directory: {root}")
    places={}
    for location, dirs, files in os.walk(root,followlinks=False):
        here=Path(location)
        level=len(here.relative_to(root).parts)
        dirs[:]=sorted(d for d in dirs if level<5 and not (here/d).is_symlink())
        f={s.lower():s for s in files if rx.fullmatch(s)}
        if not f:continue
        nums=sorted({int(rx.fullmatch(s).group(1)) for s in f})
        mandatory={f"dvdbnd{i}.{ext}" for i in (0,1) for ext in ("bhd5","bdt")}
        required={f"dvdbnd{i}.{ext}" for i in nums for ext in ("bhd5","bdt")}
        missing=sorted((mandatory|required)-set(f))
        empty=sorted(k for k,v in f.items() if (here/v).stat().st_size==0)
        gap=sorted(set(range(max(nums)+1))-set(nums))
        valid=not (missing or empty or gap)
        places[str(here)]={"found":sorted(f),"missing":missing,"empty":empty,
            "number_gaps":gap,"archive_numbers":nums,"layout_complete":valid}
    valid=[d for d,item in places.items() if item["layout_complete"]]
    chosen=valid[0] if len(valid)==1 else None
    return {"status":"PASS" if chosen else "AMBIGUOUS_ROOTS" if valid else "INVALID_SOURCE",
      "selected":chosen,"archive_pair_count":len(places[chosen]["archive_numbers"]) if chosen else 0,
      "physical_texture_coverage":"OPEN_REQUIRES_FLVER_TPF_SCAN","locations":places}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--ptde",type=Path,required=True)
    ap.add_argument("--json-out",type=Path)
    a=ap.parse_args()
    result=inspect(a.ptde)
    if a.json_out:
        a.json_out.parent.mkdir(parents=True,exist_ok=True)
        a.json_out.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print("PTDE_BHD5_SOURCE_LAYOUT",result["status"],result["archive_pair_count"],
          "pairs. TEXTURE COVERAGE OPEN")
    return 0 if result["status"]=="PASS" else 2
if __name__=="__main__":sys.exit(main())
