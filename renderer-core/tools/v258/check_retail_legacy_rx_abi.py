#!/usr/bin/env python3
"""Original-retail DSR DX11 Legacy census + exact RX33-35 ISGN/OSGN gate.

Requires owner-provided PART02 zip; no original assets are distributed.  This
proves DX11 receiver ABI, not PTDE operator/pixel equivalence.  Exit nonzero
on a wrong receiver or incompatible compiled candidate to preserve fail-open.
"""
import argparse
import hashlib
import json
import re
import struct
import zipfile
import zlib
from pathlib import Path

OWNER_PATH = "DSR/shader/FRPG_FlverPBL_fpo_DX11.shaderbnd.dcx"
RX = {
  33: "7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",
  34: "857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",
  35: "9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",
}

def chunks(blob):
    if blob[:4] != b"DXBC" or len(blob) < 36 or struct.unpack_from("<I",blob,24)[0] != len(blob):
        raise ValueError("invalid DXBC container")
    n=struct.unpack_from("<I",blob,28)[0]
    if not 1 <= n <= 64 or 32+4*n > len(blob):
        raise ValueError("invalid DXBC chunk count")
    out={}
    for k in range(n):
        p=struct.unpack_from("<I",blob,32+4*k)[0]
        if p+8 > len(blob):
            raise ValueError("invalid chunk offset")
        size=struct.unpack_from("<I",blob,p+4)[0]
        if p+8+size > len(blob):
            raise ValueError("invalid chunk length")
        key=blob[p:p+4].decode("ascii")
        if key in out:
            raise ValueError("duplicate DXBC chunk")
        out[key]=blob[p+8:p+8+size]
    return out

def signature(blob):
    if len(blob)<8:
        raise ValueError("missing signature header")
    n=struct.unpack_from("<I",blob,0)[0]
    if n>64 or 8+24*n>len(blob):
        raise ValueError("invalid signature count")
    out=[]
    for i in range(n):
        name_off, index, system_value, kind, reg, masks = struct.unpack_from("<6I",blob,8+i*24)
        if name_off >= len(blob):
            raise ValueError("invalid semantic string")
        semantic=blob[name_off:].split(b"\x00",1)[0].decode("ascii")
        out.append({"semantic":semantic,"index":index,"system_value":system_value,
                    "type":kind,"register":reg,"mask":masks&255})
    return out

def audit(path):
    with zipfile.ZipFile(path) as z:
        dcx=z.read(OWNER_PATH)
    if not dcx.startswith(b"DCX\x00"):
        raise ValueError("not a retail DCX header")
    data=zlib.decompress(dcx[76:])
    if not data.startswith(b"BND3"):
        raise ValueError("not a retail BND3 archive")
    texts=[s.decode("ascii") for s in re.findall(rb"[\x20-\x7e]{4,}",data)]
    names={s for s in texts if s.endswith(".fpo")}
    legacy={s for s in texts if "legacy" in s.lower()}
    if not any("CalcPointLightsLegacy" in s for s in legacy):
        raise ValueError("native Legacy forward source missing")
    if not any("CalcPointLightsClustered" in s for s in texts):
        raise ValueError("native Clustered source missing")
    if not any("evaluateIBLSpecular" in s for s in texts):
        raise ValueError("native PBL IBL source missing")
    if not any("if (gFC_MaterialWorkflow.x == 0)" in s for s in texts):
        raise ValueError("MaterialWorkflow branch missing")
    rev={digest:rx for rx,digest in RX.items()}
    found={}
    p=0
    while (p:=data.find(b"DXBC",p))>=0:
        if p+32<=len(data):
            size=struct.unpack_from("<I",data,p+24)[0]
            if 64<=size<=200000 and p+size<=len(data):
                blob=data[p:p+size]
                digest=hashlib.sha256(blob).hexdigest()
                if digest in rev:
                    rx=rev[digest]
                    if rx in found:
                        raise ValueError("duplicate exact RX")
                    c=chunks(blob)
                    if not {"ISGN","OSGN","RDEF","SHEX"}<=set(c):
                        raise ValueError("required DXBC sections absent")
                    found[rx]={"sha256":digest,"byte_count":size,
                               "input":signature(c["ISGN"]),"output":signature(c["OSGN"])}
        p+=4
    if set(found)!=set(RX):
        raise ValueError("authenticated retail RX missing")
    for rx,row in found.items():
        outs=[(x["semantic"],x["index"]) for x in row["output"]]
        if outs != [("SV_Target",0),("SV_Target",1)]:
            raise ValueError("unexpected stock dual-target output")
    return {
      "schema":"dsrrl.v266.retail_dx11_legacy_native_rx_abi.v1",
      "scope":"ORIGINAL_DSR_RETAIL_NOT_R19",
      "dcx_sha256":hashlib.sha256(dcx).hexdigest(),
      "pixel_status":"OPEN",
      "fpo_names":len(names),
      "hemenvlerp_fpo_names":sum("HemEnvLerp" in s for s in names),
      "standalone_legacy_fpo_names":sum("legacy" in s.lower() for s in names),
      "source_legacy_scope":"PointLight forward/falloff, not full PTDE HemEnvLerp",
      "materialworkflow_scope":"Both branches still flow through DSR PBL/IBL/Roughness",
      "original_stock_receivers":{str(k):v for k,v in sorted(found.items())},
      "proposed_bridge":"reuse host DX11 receivers, draw-specific PTDE pre-Fog operator island, preserve stock Fog/LS and BOTH target outputs",
      "not_proven":["compiled R19 identity","PTDE signal producer","FLVER+MTD+slot+sidecar authority","runtime","pixel"],
    }

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--part02",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--candidate-dxbc",type=Path)
    ap.add_argument("--candidate-rx",type=int,choices=sorted(RX))
    a=ap.parse_args()
    data=audit(a.part02)
    if a.candidate_dxbc:
        if a.candidate_rx is None:
            ap.error("--candidate-rx required with --candidate-dxbc")
        blob=a.candidate_dxbc.read_bytes()
        c=chunks(blob)
        if "ISGN" not in c or "OSGN" not in c:
            raise ValueError("candidate missing ISGN or OSGN")
        stock=data["original_stock_receivers"][str(a.candidate_rx)]
        accepted=(signature(c["ISGN"])==stock["input"] and
                  signature(c["OSGN"])==stock["output"])
        data["candidate"]={"sha256":hashlib.sha256(blob).hexdigest(),
                           "rx":a.candidate_rx,"native_abi_compatible":accepted}
        if not accepted:
            raise ValueError("FAIL_OPEN: candidate DXBC native RX ABI mismatch")
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(data,indent=2,sort_keys=True)+"\n",encoding="utf8")
    print("V266 ORIGINAL DX11 LEGACY RX33/34/35 ABI PASS; runtime and pixel OPEN")

if __name__=="__main__":
    main()
