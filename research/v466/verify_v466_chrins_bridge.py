#!/usr/bin/env python3
"""Read-only SHA-locked V466 ChrIns observer entry/vtable audit.

Usage:
  python research/v466/verify_v466_chrins_bridge.py path/to/DarkSoulsRemastered.exe --mutations

This proves native call targets and direct field reads, not in-game hook hits,
gameplay actor ownership, receiver coverage, PTDE behavior, or pixel equality.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

EXE_SHA256="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
VTABLES={
    "PlayerIns":0x13251F0,
    "EnemyIns":0x1322E68,
    "ChrIns":0x131E008,
}
WITNESSES={
    "entry_13_bytes":(0x321CE0,bytes.fromhex("48895c2418574881ecc0000000")),
    "owner_model_load":(0x321D05,bytes.fromhex("488b4960")),
    "owner_other_field_test":(0x321D12,bytes.fromhex("48837f6800")),
}
TARGET_RVA=0x321CE0

def parse_pe(raw):
    pe=struct.unpack_from("<I",raw,0x3c)[0]
    assert raw[pe:pe+4]==b"PE\x00\x00","not a PE"
    sections=struct.unpack_from("<H",raw,pe+6)[0]
    optional_size=struct.unpack_from("<H",raw,pe+20)[0]
    assert struct.unpack_from("<H",raw,pe+24)[0]==0x20b,"not PE32+"
    base=struct.unpack_from("<Q",raw,pe+24+24)[0]
    head=pe+24+optional_size
    ranges=[]
    for i in range(sections):
        p=head+40*i
        vsize,rva,rawsize,offset=struct.unpack_from("<IIII",raw,p+8)
        ranges.append((rva,rawsize,offset))
    return base,ranges

def offset_for(rva,ranges):
    for start,rawsize,offset in ranges:
        if start<=rva<start+rawsize:return offset+rva-start
    raise AssertionError("RVA outside raw sections: "+hex(rva))

def verify_witnesses(raw,base,ranges):
    result={}
    for kind,vt in VTABLES.items():
        va=struct.unpack_from("<Q",raw,offset_for(vt+0x120,ranges))[0]
        assert va==base+TARGET_RVA, f"{kind} vtable+0x120 mismatch {va:#x}"
        result[kind+"_slot_120_rva"]=hex(va-base)
    for kind,(rva,needle) in WITNESSES.items():
        off=offset_for(rva,ranges)
        assert raw[off:off+len(needle)]==needle,kind+" preimage mismatch"
        result[kind]=needle.hex()
    return result

def main():
    p=argparse.ArgumentParser()
    p.add_argument("exe",type=Path)
    p.add_argument("--mutations",action="store_true")
    a=p.parse_args()
    raw=a.exe.read_bytes()
    sha=hashlib.sha256(raw).hexdigest()
    assert sha==EXE_SHA256,"wrong original DSR EXE; fail-open"
    base,ranges=parse_pe(raw)
    witnesses=verify_witnesses(raw,base,ranges)
    negatives=0
    if a.mutations:
        points=[offset_for(v+0x120,ranges) for v in VTABLES.values()]
        points += [offset_for(rva,ranges) for rva,_ in WITNESSES.values()]
        for off in points:
            changed=bytearray(raw)
            changed[off]^=1
            try:verify_witnesses(changed,base,ranges)
            except AssertionError:negatives+=1
            else:raise AssertionError("mutation not rejected at file offset "+hex(off))
    print(json.dumps({
        "status":"STATIC_RE_PASS", "sha256":sha,
        "target_rva":hex(TARGET_RVA), "witnesses":witnesses,
        "negative_mutations_passed":negatives,
        "runtime_owner":"OPEN","rx34_pixel":"OPEN","ptde_pixel":"OPEN",
        "addon_exe_disk_patch":False,
    },indent=2))

if __name__=="__main__":main()
