#!/usr/bin/env python3
"""Independent V300 donor origin verification, with optional vanilla ZIP."""
from pathlib import Path
import argparse,csv,hashlib,re,struct,zipfile,zlib

ROOT=Path(__file__).resolve().parents[1]
TABLE=ROOT/"data/provenance/v300_lightbank_donors_ptde_dsr_only.tsv"
LAYOUT=ROOT/"data/provenance/pmetal_env_source_layout_v1.tsv"
AUTH=ROOT.parent/"runtime-v1/generated/v13_pmetal_donors.hpp"
SOURCE_SHA="70a90b51e471e8330b55ef17a9b8b86e18f7c0a1cfdb7384f3f51cfe165b8372"
assert TABLE.is_file() and LAYOUT.is_file() and AUTH.is_file()
def readtab(p):
    with p.open(encoding="utf-8",newline="") as f:
        return list(csv.DictReader((line for line in f if not line.startswith("#")),delimiter="\t"))
rows=readtab(TABLE)
layout=readtab(LAYOUT)
a=AUTH.read_text(encoding="utf-8")
raw=[tuple(map(int,m.groups())) for m in re.finditer(r"donor_row\{(\d+)u,(\d+)u,(\d+)u,(\d+)u,(\d+)u\}",a)]
banks=[(int(m.group(1),16),int(m.group(2)),int(m.group(3))) for m in re.finditer(r"bank_donor\{0x([0-9a-fA-F]+)ULL,(\d+)u,(\d+)u\}",a)]
assert len(rows)==1249 and len(raw)==1246 and len(banks)==20 and len(layout)==20
extra=[]
for i,(sig,first,count) in enumerate(banks):
    lm=layout[i]
    assert int(lm["bank_index"])==i and int(lm["v13_signature"],16)==sig
    assert count==int(lm["donor_count"])
    scoped=[x for x in rows if int(x["bank_index"])==i]
    assert len(scoped)==int(lm["live_count"])
    assert [int(x["row_id"]) for x in scoped]==list(range(int(lm["live_count"])))
    for j in range(count):
        v=scoped[j]
        assert v["origin"]=="PTDE" and int(v["bank_v13_signature"],16)==sig
        assert v["dsr_layout_signature"]==lm["layout_signature"]
        assert [int(v[k]) for k in ("envspec_r","envspec_g","envspec_b","envspec_m")]==list(raw[first+j][1:])
        assert v["envdiffuse_ptde_reference"]=="PTDE_ENVDIFFUSE_DONOR_V1.bin:"+lm["v13_signature"]+":"+str(j)
        assert all(v[k]=="" for k in ("envdiffuse_r","envdiffuse_g","envdiffuse_b","envdiffuse_m"))
    for v in scoped[count:]:
        assert v["origin"]=="DSR_ONLY" and int(v["row_id"])==64
        extra.append((lm,v))
assert len(extra)==3 and {x[0]["bank_name"] for x in extra}=={"m14_LightBank.param","s14_LightBank.param","m18_LightBank.param"}

ap=argparse.ArgumentParser()
ap.add_argument("--dsr-zip",type=Path)
args=ap.parse_args()
if args.dsr_zip:
    binary=args.dsr_zip.read_bytes()
    assert hashlib.sha256(binary).hexdigest()==SOURCE_SHA
    with zipfile.ZipFile(args.dsr_zip) as z:
        for m,v in extra:
            blob=z.read("DrawParam/"+m["area"]+"_DrawParam.parambnd.dcx")
            encoded=zlib.decompress(blob[blob.index(b"\x78\xda"):])
            assert encoded[:4]==b"BND3"
            n=struct.unpack_from("<I",encoded,0x10)[0]
            found=[]
            for i in range(n):
                offset=0x20+24*i
                size,ptr=struct.unpack_from("<II",encoded,offset+4)
                name_ptr=struct.unpack_from("<I",encoded,offset+16)[0]
                name=encoded[name_ptr:encoded.find(b"\x00",name_ptr)].decode("ascii").split("\\")[-1]
                if name==m["bank_name"]:
                    found.append(encoded[ptr:ptr+size])
            assert len(found)==1
            data=found[0]
            assert hashlib.sha256(data).hexdigest()==m["param_sha256"]
            assert struct.unpack_from("<H",data,10)[0]==65
            ri=int(v["row_id"])
            rowptr=struct.unpack_from("<I",data,0x30+12*ri+4)[0]
            nums=struct.unpack_from("<46h",data,rowptr)
            diff=list(nums[32:36])
            spec=list(nums[36:40])
            assert diff==[int(v[k]) for k in ("envdiffuse_r","envdiffuse_g","envdiffuse_b","envdiffuse_m")]
            assert spec==[int(v[k]) for k in ("envspec_r","envspec_g","envspec_b","envspec_m")]
            print(m["area"],m["bank_name"],"row64", "DSR-ONLY", "EnvDiffuse",diff,"EnvSpec",spec)
print("V300_DONOR_SOURCE_PASS: 1246 PTDE + 3 EXACT_DSR_ONLY; unknown/future rows NOT authorized")
