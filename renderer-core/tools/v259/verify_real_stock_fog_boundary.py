#!/usr/bin/env python3
"""V259 portable offline source proof for stock DSR RX33/34/35 Fog cut.

Takes original DSR DXBC files supplied by the game owner (never redistributes
them), fingerprints the exact 212-DWORD atmosphere region and falsifies the
gate with changed Fog/LightScattering instructions. Does NOT infer PTDE pixels.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

RECEIVERS = {
  33: ("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",2904,2552,2764),
  34: ("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",2823,2471,2683),
  35: ("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",2476,2124,2336),
}
EXPECTED_FOG_SHA = "bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0"

def shex(source):
    if source[:4] != b"DXBC" or len(source)<40:
        raise ValueError("not DXBC")
    size, count = struct.unpack_from("<II",source,24)
    if size != len(source) or not 1 <= count <= 64:
        raise ValueError("invalid DXBC header")
    if 32+4*count>len(source):
        raise ValueError("invalid chunk directory")
    code = []
    for i in range(count):
        off=struct.unpack_from("<I",source,32+4*i)[0]
        if off+8>len(source):
            raise ValueError("invalid chunk offset")
        tag=source[off:off+4]
        sz=struct.unpack_from("<I",source,off+4)[0]
        if off+8+sz>len(source):
            raise ValueError("invalid chunk span")
        if tag in (b"SHEX",b"SHDR"):
            if sz%4:raise ValueError("bad code alignment")
            code.append((off+8,list(struct.unpack_from("<"+"I"*(sz//4),source,off+8))))
    if len(code)!=1:raise ValueError("missing/nonunique SHEX")
    start,words=code[0]
    if len(words)<2 or words[1]!=len(words):raise ValueError("bad SHEX length")
    return start,words

def code_positions(words):
    i=2;out=[]
    while i<len(words):
        length=(words[i]>>24)&127
        if not length or i+length>len(words):
            raise ValueError("invalid instruction length")
        out.append((i,i+length,words[i:i+length]))
        i+=length
    if i!=len(words):raise ValueError("bad instruction decode")
    return out

def verify(rx, original, candidate):
    sha,terminal,start,end=RECEIVERS[rx]
    if hashlib.sha256(original).hexdigest()!=sha:
        raise ValueError("stock receiver digest mismatch")
    if start!=terminal-352 or end!=terminal-140 or end-start!=212:
        raise ValueError("invalid certified receiver cut")
    _,sw=shex(original)
    _,cw=shex(candidate)
    origin_ops=code_positions(sw)
    _=code_positions(cw)
    if not any(a==start for a,_,_ in origin_ops):
        raise ValueError("Fog boundary is not an instruction")
    if not any(b==end for _,b,_ in origin_ops):
        raise ValueError("LightScattering boundary is not an instruction")
    payload=struct.pack("<"+"I"*212,*sw[start:end])
    if hashlib.sha256(payload).hexdigest()!=EXPECTED_FOG_SHA:
        raise ValueError("wrong stock atmosphere instructions")
    refs=[]
    for a,b,w in origin_ops:
        if a<start or b>end:continue
        for j in range(len(w)-2):
            if (w[j]>>12)&255==8 and w[j+1]==0 and 12<=w[j+2]<=20:
                refs.append(w[j+2])
    if len(refs)!=14 or refs.count(12)!=2 or 20 not in refs:
        raise ValueError("stock Fog/LightScattering CB census failed")
    span=sw[start:end]
    hits=sum(cw[i:i+212]==span for i in range(2,len(cw)-211))
    if hits!=1:raise ValueError("stock Fog region absent or nonunique")
    return {"receiver":rx,"original_sha256":sha,
            "terminal_word":terminal,"fog_start_word":start,
            "last_atmosphere_end_word":end,"fog_ls_words":212,
            "fog_ls_sha256":EXPECTED_FOG_SHA,
            "fog_ls_constant_references":len(refs),
            "stock_fog_window_preserved":True,
            "ptde_pixel_equivalence":"NOT_TESTED"}

def smoke(rx,data):
    _,words=shex(data)
    assert verify(rx,data,data)["stock_fog_window_preserved"]
    # Intentional mutation outside atmosphere: accepted by the region
    # preservation test only; NOT a valid replacement DXBC/checksum.
    outside=bytearray(data)
    offset,_=shex(outside)
    outside[offset+4*64]^=1
    assert verify(rx,data,bytes(outside))["stock_fog_window_preserved"]
    # An actual Fog operand/instruction mutation must fail the guard.
    _,_,start,_=RECEIVERS[rx]
    changed=bytearray(data)
    changed[offset+4*(start+6)]^=1
    try:
        verify(rx,data,bytes(changed))
    except ValueError:
        pass
    else:
        raise AssertionError("Fog mutation escaped V259 guard")

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--donors",type=Path,required=True,
                    help="directory with RX33.dxbc RX34.dxbc RX35.dxbc, supplied locally only")
    ap.add_argument("--out",type=Path)
    a=ap.parse_args()
    records=[]
    for rx in RECEIVERS:
        data=(a.donors/f"RX{rx}.dxbc").read_bytes()
        smoke(rx,data)
        records.append(verify(rx,data,data))
    output={"schema":"dsrrl.v259.stock_fog_guard_census.v1",
            "source":"owner local stock DSR shaderbnd; no original DXBC distributed",
            "scope":"RX33-35 only, stock Fog+LightScattering 212-word window",
            "fingerprint_shared":EXPECTED_FOG_SHA,
            "record_count":len(records),"records":records,
            "runtime":"OPEN","pixel":"OPEN"}
    if a.out:
        a.out.parent.mkdir(parents=True,exist_ok=True)
        a.out.write_text(json.dumps(output,indent=2)+"\n",encoding="utf8")
    print("V259_STOCK_DSR_FOG_LS_THREE_REAL_DONORS_PASS")
    print(json.dumps({"receivers":list(RECEIVERS),
        "words_per_receiver":212,"region_sha256":EXPECTED_FOG_SHA}))

if __name__=="__main__":main()
