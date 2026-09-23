#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path

EXPECTED = {
    "v151":"7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc",
    "v153":"4641d2f9d66e879cc86c05aa403a6f9fcfe96c5f9486c15cdae4339f15d50d68",
    "v154":"4ddf2b270b09efcc57c4cf680ac0da78ebc3f0a5f58fd7d242b87b90aa6168cc",
    "v155":"bf552c8606e27ab1bf3535b27509e91d8692fe553993a0e80b767cd30c1dd283",
    "v156":"9f112214a7cfc058775df26c58d4e8afaaa40fa22015613520bcaafcc9d5d21b",
    "v157":"cfd1fe585710497a411adad8dc7edd9b46ae187133ee0625abeed2ab1ab96f09",
}
PATCHES = {
"v153":[
(0x195470,"4cb40100","9cf3ffff"),(0x195485,"87cd0100","07ffffff"),(0x1954f8,"f4b20100","74eaffff"),
(0x1b7834,"6163746976652065786163742d736c","5631352e332073746174652d6c6179"),
(0x1b7844,"742062696e6420646961676e6f73746963","757420706978656c2d696e657274000000")],
"v154":[(0x1b05a8,"488b9688221100","e9150100009090")],
"v155":[
(0x1b1cf6,"0023","6022"),(0x1b1d90,"0423","6422"),(0x1b1dac,"0423","6422"),(0x1b1e42,"0023","6022"),
(0x1b23c3,"0823","6822"),(0x1b23d4,"0923","6922"),(0x1b23e6,"0a23","6a22"),(0x1b23f8,"0b23","6b22"),
(0x1b240a,"0c23","6c22"),(0x1b241c,"0d23","6d22"),(0x1b242e,"0e23","6e22"),(0x1b2440,"0f23","6f22"),
(0x1b2452,"1023","7022"),(0x1b2464,"1123","7122"),(0x1b2476,"1223","7222"),(0x1b2488,"1323","7322"),
(0x1b249a,"1423","7422"),(0x1b24ac,"1523","7522"),(0x1b24be,"1623","7622")],
"v156":[
(0x1b05a8,"e9150100009090","488b9688221100"),
(0x1b42a2,"0823","6822"),(0x1b42ad,"0923","6922"),(0x1b42b9,"0a23","6a22"),(0x1b42c5,"0b23","6b22"),
(0x1b42d1,"0c23","6c22"),(0x1b42dd,"0d23","6d22"),(0x1b42e9,"0e23","6e22"),(0x1b42f5,"0f23","6f22"),
(0x1b4301,"1023","7022"),(0x1b430d,"1123","7122"),(0x1b4319,"1223","7222"),(0x1b4325,"1323","7322"),
(0x1b4331,"1423","7422"),(0x1b433d,"1523","7522"),(0x1b4349,"1623","7622")],
"v157":[
(0x195470,"9cf3ffff","4cb40100"),(0x195485,"07ffffff","87cd0100"),(0x1954f8,"74eaffff","f4b20100"),
(0x1b7838,"33","37"),
(0x1b783a,"73746174652d6c61796f757420706978656c2d696e657274","616374697665204253532d73616665000000000000000000")]
}
SEMANTICS={
"v153":"restore PRE/PREPARE/POST to V12 targets; preserve V15.1 state/init/formatter layout",
"v154":"bypass V15.1 per-thread A/B semantic-store block",
"v155":"relocate colliding V15 static/BSS references 0x112300..0x112316 -> 0x112260..0x112276",
"v156":"re-enable thread-state store and relocate missed unload/uninit BSS references",
"v157":"reactivate exact-slot EnvSpec PRE/PREPARE/POST wrappers on BSS-safe V15.6 basis",
}
def sha(b): return hashlib.sha256(b).hexdigest()
def apply(data, stage):
    b=bytearray(data)
    for off,oldh,newh in PATCHES[stage]:
        old,new=bytes.fromhex(oldh),bytes.fromhex(newh)
        got=bytes(b[off:off+len(old)])
        if got!=old: raise SystemExit(f"{stage}: guard mismatch at 0x{off:x}: {got.hex()} != {old.hex()}")
        b[off:off+len(new)]=new
    out=bytes(b)
    if sha(out)!=EXPECTED[stage]: raise SystemExit(f"{stage}: SHA mismatch {sha(out)}")
    return out
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("v151"); ap.add_argument("--out-dir",default="reconstructed")
    ns=ap.parse_args(); cur=Path(ns.v151).read_bytes()
    if sha(cur)!=EXPECTED["v151"]: raise SystemExit("V15.1 base SHA mismatch")
    out=Path(ns.out_dir); out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for st in ["v153","v154","v155","v156","v157"]:
        cur=apply(cur,st); (out/f"{st}.addon64").write_bytes(cur)
        rows.append({"stage":st,"sha256":sha(cur),"size":len(cur),"patch_count":len(PATCHES[st]),"semantics":SEMANTICS[st]})
        print(st,sha(cur),"PASS")
    (out/"RECONSTRUCTION_AUDIT.json").write_text(json.dumps({
      "schema":"dsrrl.material_response_1_45.envspec_v151_v157_exact_reconstruction.v1",
      "base_v151_sha256":EXPECTED["v151"],"stages":rows,"final_v157_sha256":EXPECTED["v157"],
      "status":"EXACT_BINARY_RECONSTRUCTION_PASS","source_complete":False,
      "note":"Closes exact historical binary-delta provenance V15.1->V15.7; does not replace missing original V15.1 C source or pre-V12 source chain."
    },indent=2)+"\n",encoding="utf-8")
if __name__=="__main__": main()
