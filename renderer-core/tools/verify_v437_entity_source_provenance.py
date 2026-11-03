#!/usr/bin/env python3
"""V437 compact verifier: typed entity DrawParam -> native producer -> model draw.
Read-only, SHA-locked to the owner's unmodified original DSR x64 executable.
Expanded verifier and mutation fixtures are in the V437 handoff archive.
"""
from pathlib import Path
import argparse, hashlib, json, struct

SHA = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
OPS = {
    0x0c04a0:"0fb701668902488bc2c3", 0x0c0550:"0fb74102668902488bc2",
    0x220111:"488bf9", 0x2201a6:"488975d0",
    0x220265:"6689551c", 0x220273:"f30f108714010000",
    0x22028b:"f30f114520", 0x2202a5:"66894d1e",
    0x2203db:"3c04", 0x2203dd:"0f94c0", 0x2203e0:"888520010000",
    0x220614:"488d9f08010000", 0x22061b:"f30f109f4c010000",
    0x22062b:"4c8bc3", 0x22062e:"3c04", 0x220630:"7507",
    0x220cd3:"488b4f40", 0x220ce6:"488d55d0", 0x220ced:"ff5018",
    0x1c0be4:"4d8bc8", 0x1c0c14:"4d8bc8",
    0x1c6930:"498d4908", 0x1c697b:"41b805000000",
    0x1c6c70:"498d4908", 0x1c6cbb:"41b806000000",
    0x2167ad:"4c8d8708010000",
}
CALLS = {
    0x220256:0xc04a0, 0x22026e:0xc0550,
    0x220632:0x1c0c10, 0x220639:0x1c0be0,
    0x1c0bf5:0x1c6900, 0x1c0c25:0x1c6c40,
    0x1c693f:0xc04a0, 0x1c6951:0xc0550,
    0x1c6c7f:0xc04a0, 0x1c6c91:0xc0550,
    0x1c69fe:0x563b80, 0x1c6a97:0x563b80,
    0x1c6d55:0x563b80, 0x1c6dee:0x563b80,
    0x1c6bc5:0x563c30, 0x1c6f4a:0x563c30,
    0x2167ba:0x1c0be0, 0x220839:0x20fee0,
}
FULL = {
    0x563b80:{0x1c69fe,0x1c6a97,0x1c6d55,0x1c6dee},
    0x563c30:{0x1c6bc5,0x1c6f4a},
    0x1c6900:{0x1c0bf5}, 0x1c6c40:{0x1c0c25},
    0x1c0be0:{0x2167ba,0x220639}, 0x1c0c10:{0x220632},
}
def check(path):
    data=Path(path).read_bytes()
    assert hashlib.sha256(data).hexdigest()==SHA, "Original EXE SHA mismatch"
    pe=struct.unpack_from("<I",data,0x3c)[0]
    assert data[pe:pe+4]==b"PE"+bytes(2)
    count=struct.unpack_from("<H",data,pe+6)[0]
    sec=pe+24+struct.unpack_from("<H",data,pe+20)[0]
    parts=[]
    for i in range(count):
        s=sec+40*i
        name=data[s:s+8].split(bytes([0]))[0].decode("ascii","replace")
        _,rva,size,offset=struct.unpack_from("<IIII",data,s+8)
        parts.append((name,rva,size,offset))
    def get(rva,size):
        for _,base,length,offset in parts:
            if base<=rva and rva+size<=base+length:
                return data[offset+rva-base:offset+rva-base+size]
        raise AssertionError("Unmapped RVA "+hex(rva))
    for src,expected in OPS.items():
        raw=bytes.fromhex(expected)
        assert get(src,len(raw))==raw, "opcode "+hex(src)
    for src,target in CALLS.items():
        raw=get(src,5)
        assert raw[0]==0xe8 and src+5+struct.unpack_from("<i",raw,1)[0]==target, "CALL "+hex(src)
    census={t:set() for t in FULL}
    for name,base,length,offset in parts:
        if name!=".text":continue
        blob=data[offset:offset+length]
        at=0
        while True:
            i=blob.find(bytes([0xe8]),at)
            if i<0 or i+5>len(blob):break
            target=base+i+5+struct.unpack_from("<i",blob,i+1)[0]
            if target in census:census[target].add(base+i)
            at=i+1
    assert census==FULL,"native direct producer call-site census"
    assert 0x108+8==0x110 and 0x108+10==0x112
    assert 0x108+12==0x114 and -0x30+0x150==0x120
    return {"version":"V437","preimages":len(OPS),"calls":len(CALLS),
            "direct_target_censuses":len(FULL),"static_source_to_model":"CONFIRMED",
            "same_invocation_native_sequence":"OPEN","RX34":"OPEN","pixels":"OPEN"}
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("exe")
    a=p.parse_args()
    print(json.dumps(check(a.exe),indent=2))
