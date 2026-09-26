#!/usr/bin/env python3
from __future__ import annotations
import argparse,csv,hashlib,re,struct
from pathlib import Path

DSR_RE=re.compile(r"^\d+_(FRPG_Phn_DifSpc.*HemEnvPntSS(?:SS)?\.fpo)$")
PTDE_RE=re.compile(r"^(FRPG_Phn_DifSpc.*HemEnv(?:Lerp)?PntSS(?:SS)?\.fpo)$")
SRC_SHA="57314b36812dfa2867512052278a08dc0da8a41ea0414c1fbbe4ccdf45838076"

def sha(p:Path)->str: return hashlib.sha256(p.read_bytes()).hexdigest()

def main()->int:
    ap=argparse.ArgumentParser()
    ap.add_argument("--dsr-dir",required=True)
    ap.add_argument("--ptde-dir",required=True)
    ap.add_argument("--output",required=True)
    a=ap.parse_args()
    dsr={}
    for p in Path(a.dsr_dir).glob("*.fpo"):
        m=DSR_RE.match(p.name)
        if m: dsr[m.group(1)]=p
    ptde={p.name:p for p in Path(a.ptde_dir).glob("*.fpo") if PTDE_RE.match(p.name)}
    rows=[]
    for label,p in sorted(dsr.items()):
        q=Path(label.replace("HemEnvPnt","HemEnvLerpPnt"))
        if label not in ptde or q.name not in ptde: continue
        rows.append({
          "label":label,
          "receiver_class":"fixed_spc_pntssss" if "PntSSSS" in label else "fixed_spc_pntss",
          "dsr_size":p.stat().st_size,
          "dsr_sha256":sha(p),
          "ptde_hemenv_size":ptde[label].stat().st_size,
          "ptde_hemenv_sha256":sha(ptde[label]),
          "ptde_hemenvlerp_size":ptde[q.name].stat().st_size,
          "ptde_hemenvlerp_sha256":sha(ptde[q.name]),
          "source_zip_sha256":SRC_SHA,
        })
    if len(rows)!=48 or len({r["dsr_sha256"] for r in rows})!=48:
        raise SystemExit(f"expected 48 unique fixed DSR bodies, got rows={len(rows)} unique={len({r['dsr_sha256'] for r in rows})}")
    fields=list(rows[0])
    with Path(a.output).open("w",newline="",encoding="utf-8") as f:
        w=csv.DictWriter(f,fieldnames=fields,delimiter="\t")
        w.writeheader();w.writerows(rows)
    print(f"FIXED_PTDE_PAIR_PROVENANCE_PASS rows={len(rows)} unique_dsr=48")
    return 0

if __name__=="__main__":
    raise SystemExit(main())
