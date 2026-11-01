#!/usr/bin/env python3
"""V435 compact in-repository verifier; expanded V435 ZIP audits 84 sites and V434."""
import hashlib, json, struct, sys
from pathlib import Path

SHA = "a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
IMAGE = 0x140000000
OPS = {
  0x22ba74:"e8d7a40600", 0x22ba8c:"e99f05fcff",
  0x295f9f:"4863ca", 0x295fa5:"488d0c48", 0x295fb8:"488d0c50",
  0x295fbc:"418b448a38", 0x1ec030:"4533c0", 0x1ec03e:"e9fdb1fc00",
  0x11b7288:"4c8b14c8",0x11b728c:"488d0d9d124500",
  0x11b7293:"498b02",0x11b7296:"483bc1",
  0x20e2fe:"f3410f1087e0000000",0x20e30c:"4180bff000000000",
  0x20e342:"e899cafdff",0x20e38b:"e890cffdff",
  0x1eb479:"663b8b48230000",0x1eb486:"0fb7834a230000",
  0x1eb539:"66898b48230000",0x1eb549:"6689834a230000",
  0x1eb56f:"c1fa08",0x1eb572:"83e27f",
  0x1eb575:"41b805000000",0x1eb57b:"488b8b20230000",
  0x1eb582:"e8b9e53700",0x1eb5b7:"41b805000000",
  0x1eb5c6:"e875e53700",0x569b40:"83fa0b",
  0x569b45:"4183f80c",0x569b4e:"4869c0b0010000",
  0x569b5b:"488b440820",0x569b60:"488b44d008",
  0x563de7:"0fb7480a",0x563def:"0fb6c2",
  0x563dfa:"4869c810010000",0x563e05:"488b9401e8000000",
  0x563e12:"8b4228",
}
CALLS = {0x22ba74:0x295f50,0x20e342:0x1eade0,
         0x20e38b:0x1eb320,0x1eb582:0x569b40,
         0x1eb5c6:0x569b40,0x1eaf40:0x563de0}
JUMPS = {0x22ba8c:0x1ec030,0x1ec03e:0x11b7240}

def main(path):
    data=Path(path).read_bytes()
    sha=hashlib.sha256(data).hexdigest()
    if sha!=SHA: raise ValueError("Wrong original EXE SHA256: "+sha)
    pe=struct.unpack_from("<I",data,0x3c)[0]
    if data[pe:pe+4]!=b"PE\0\0": raise ValueError("not PE")
    n=struct.unpack_from("<H",data,pe+6)[0]
    off=pe+24+struct.unpack_from("<H",data,pe+20)[0]
    sections=[]
    for i in range(n):
        p=off+40*i
        virtual_size,rva,raw_size,raw=struct.unpack_from("<IIII",data,p+8)
        sections.append((rva,rva+raw_size,raw))
    def get(rva,size):
        for start,end,raw in sections:
            if start<=rva and rva+size<=end:
                return data[raw+rva-start:raw+rva-start+size]
        raise ValueError("unmapped RVA "+hex(rva))
    for rva,expected in OPS.items():
        assert get(rva,len(expected)//2)==bytes.fromhex(expected),hex(rva)
    for table in (CALLS,JUMPS):
        for src,dst in table.items():
            b=get(src,5)
            assert b[0]==(0xe8 if table is CALLS else 0xe9)
            assert src+5+struct.unpack_from("<i",b,1)[0]==dst,hex(src)
    vt=0x1608530
    col=struct.unpack("<Q",get(vt-8,8))[0]-IMAGE
    sig,offset,cd,type_rva,cl,self_rva=struct.unpack("<IIIIII",get(col,24))
    assert sig==1 and self_rva==col
    name=get(type_rva+16,80).split(b"\0")[0].decode("ascii")
    assert name==".?AVHGShaderSetEntity@DLHG@@",name
    direct={}
    for label,start,end in (("full",0x20dda0,0x20e9d0),
                            ("alternate_1",0x20e9d0,0x20f130),
                            ("alternate_2",0x20f9f0,0x20fee0)):
        raw=get(start,end-start)
        found=[]
        for i in range(len(raw)-4):
            if raw[i]==0xe8:
                dst=start+i+5+struct.unpack_from("<i",raw,i+1)[0]
                if dst in (0x1eade0,0x1eb320):found.append([hex(start+i),hex(dst)])
        direct[label]=found
    assert direct["full"]==[["0x20e342","0x1eade0"],["0x20e38b","0x1eb320"]]
    assert not direct["alternate_1"] and not direct["alternate_2"]
    return dict(exe_sha=sha,opcodes=len(OPS),calls=len(CALLS),jumps=len(JUMPS),
                shader_set_rtti=name,lightbank_direct_call_census=direct,
                runtime="OPEN",pixels="OPEN")

if __name__=="__main__":
    result=main(sys.argv[1])
    print(json.dumps(result,indent=2))
