#!/usr/bin/env python3
"""Read-only P_Metal RX33/RX34 native PS b0 consumer audit.

Requires the exact user-provided DSR FlverPBL fpo .shaderbnd.dcx and
a PR292 register-hash JSON. No game binaries/assets are stored in Git.
DXBC dcl_constantbuffer is NOT a shader read. Dynamic CB indexing is
reported separately, never silently treated as a fixed register.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import zlib

BINDER_SHA = "ad180732ac79d5d98783aa504c789c2bab15e515c3b0f66b237d8b8c69113394"
SHADERS = {
    33: (894, "35880c0b2f2330208dfc21af6dd3d944218fcc4540cd8e59404a0aefc13c0b24"),
    34: (913, "d6038de494509e7cbcbfb904c4046e9427f3b921f6a35735a0b0d316f9976837"),
}
DCL_CONSTANT_BUFFER = 89

def u32(data, offset):
    return struct.unpack_from("<I", data, offset)[0]

def name(data, offset):
    if offset < 0 or offset >= len(data):
        raise ValueError("Name offset out of range")
    end = data.find(b"\0", offset)
    if end < 0:
        raise ValueError("Unterminated name")
    return data[offset:end].decode("utf-8", "replace")

def binder_entries(path):
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != BINDER_SHA:
        raise ValueError("Exact DSR binder SHA mismatch: fail-open")
    if data[0x18:0x1c] != b"DCS\0" or data[0x24:0x28] != b"DCP\0":
        raise ValueError("Unexpected DCX envelope")
    unpacked = zlib.decompress(data[0x4c:])
    if len(unpacked) != int.from_bytes(data[0x1c:0x20], "big") or unpacked[:4] != b"BND3":
        raise ValueError("BND3/DCX validation failed")
    count = u32(unpacked, 0x10)
    result = {}
    for rx, (idx, sha) in SHADERS.items():
        if idx >= count:
            raise ValueError("Exact shader missing")
        offset, _, no, size, _, _ = struct.unpack_from("<6I", unpacked, 0x28+24*idx)
        shader = unpacked[offset:offset+size]
        if len(shader) != size or hashlib.sha256(shader).hexdigest() != sha:
            raise ValueError("Exact RX shader identity failed")
        result[rx] = (idx, name(unpacked, no), shader, sha)
    return result

def dxbc_chunks(data):
    if data[:4] != b"DXBC" or u32(data,24) != len(data):
        raise ValueError("Invalid DXBC container")
    n = u32(data,28)
    offsets = struct.unpack_from("<"+"I"*n,data,32)
    return {data[o:o+4]:data[o+8:o+8+u32(data,o+4)] for o in offsets}

def rdef_fields(data):
    n, table = struct.unpack_from("<2I",data,0)
    for j in range(n):
        no, nv, vo, width, _, _ = struct.unpack_from("<6I",data,table+24*j)
        if name(data,no) != "$Globals":
            continue
        fields = {}
        for k in range(nv):
            varname, offset, size = struct.unpack_from("<3I",data,vo+40*k)
            if size:
                for reg in range(offset//16,(offset+size-1)//16+1):
                    fields[reg] = name(data,varname)
        return width,fields
    raise ValueError("Missing native $Globals reflection")

def reads_excluding_declarations(data):
    if len(data)%4:
        raise ValueError("Unaligned DXBC shader code")
    w = struct.unpack("<"+"I"*(len(data)//4),data)
    if len(w)<2 or w[1]!=len(w):
        raise ValueError("Invalid token count")
    static = Counter()
    relative = []
    i = 2
    while i<len(w):
        opcode = w[i]&0x7ff
        length = (w[i]>>24)&0x7f
        if length<=0 or i+length>len(w):
            raise ValueError("Invalid DXBC instruction length")
        if opcode!=DCL_CONSTANT_BUFFER:
            for j in range(i+1,i+length):
                token = w[j]
                if (token>>12)&255 != 8 or (token>>20)&3 != 2:
                    continue
                mode0,mode1 = (token>>22)&7,(token>>25)&7
                if mode0!=0 or j+2>=i+length or w[j+1]!=0:
                    continue
                if mode1==0:
                    static[w[j+2]]+=1
                elif mode1==3 and j+3<i+length:
                    relative.append({"base_register":w[j+2],"mode":"immediate+relative","opcode":opcode})
                else:
                    relative.append({"base_register":None,"mode":f"index_{mode1}","opcode":opcode})
        i+=length
    return static, relative

def audit(binder, fingerprints):
    if len(fingerprints["registers"])!=129:
        raise ValueError("PR292 fingerprint count != 129")
    output={"schema":"dsrrl.pmetal_rx33_rx34_b0_consumer_v1",
            "binder_sha256":BINDER_SHA,
            "scope":"CPU Map/Unmap fingerprints only; no native GPU draw, PTDE pixel or branch authority",
            "receivers":{}}
    for rx,(idx,shader_name,shader,sha) in binder_entries(binder).items():
        chunks=dxbc_chunks(shader)
        width,fields=rdef_fields(chunks[b"RDEF"])
        direct,relative=reads_excluding_declarations(chunks.get(b"SHEX",chunks.get(b"SHDR",b"")))
        hits=[]
        for reg in sorted(direct):
            if reg>=129:
                continue
            fp=fingerprints["registers"][reg]
            hits.append({"register":reg,"field":fields.get(reg),
                         "reads":direct[reg],"flips_changed":fp["flip_changed"],
                         "same_bank_changed":fp["same_bank_changed"],
                         "fingerprints_overlap_between_banks":fp["overlap_values"]})
        output["receivers"][str(rx)]={
            "index":idx,"name":shader_name,"sha256":sha,
            "rdef_globals_size":width,"observed_b0_size":2064,
            "live_direct_registers":hits,
            "relative_cb_references":relative,
            "direct_water_c61_c62":61 in direct or 62 in direct,
            "live_direct_changed":[h for h in hits if h["flips_changed"]>0],
        }
    return output

def selftest():
    # The declaration cb0[103] is NOT an executable read of c103.
    # This synthetic opcode stream contains only one live read of c63.
    w=[0x50,14,0x04000059,0x00208e46,0,103,
       0x08000011,0x00100012,0,0x00100e46,1,0x00208e46,0,63]
    stat,dyn=reads_excluding_declarations(struct.pack("<"+"I"*len(w),*w))
    assert stat==Counter({63:1}) and dyn==[]
    print("PASS: DXBC declaration excluded, executable CB0 read retained")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binder",type=Path)
    p.add_argument("--fingerprints",type=Path)
    p.add_argument("--output",type=Path)
    p.add_argument("--selftest",action="store_true")
    a=p.parse_args()
    if a.selftest:
        selftest()
        if not a.binder: return
    if not a.binder or not a.fingerprints:
        p.error("Provide both --binder and --fingerprints")
    result=audit(a.binder,json.loads(a.fingerprints.read_text(encoding="utf-8")))
    content=json.dumps(result,ensure_ascii=False,indent=2)+"\n"
    if a.output:
        a.output.write_text(content,encoding="utf-8")
        print(f"PASS: RX33/RX34 audited; output={a.output}")
    else:
        print(content)

if __name__=="__main__":
    main()
