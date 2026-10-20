#!/usr/bin/env python3
"""Pure synthetic BND3->TPF->DDS positive and negative content identity test."""
from pathlib import Path
import sys
import struct
import hashlib
sys.path.insert(0,str(Path(__file__).resolve().parent))
import match_v230_bc1_mip0_dsr_archive as match

def report():
    return {"sha256":"","containers":0,"dds_seen":0,"bc1_candidates":0,
            "invalid_dcx":0,"matches":[]}

dds=bytearray(128+32768)
dds[:4]=b"DDS "
struct.pack_into("<I",dds,4,124)
struct.pack_into("<I",dds,12,256)
struct.pack_into("<I",dds,16,256)
struct.pack_into("<I",dds,28,9)
dds[84:88]=b"DXT1"
top=bytes((i*23+i//31)%256 for i in range(32768))
dds[128:]=top
tpf=bytearray(0x40)
tpf[:4]=b"TPF\0"
struct.pack_into("<I",tpf,8,1)
struct.pack_into("<IIIII",tpf,0x10,0x40,len(dds),0,0x24,0)
tpf[0x24:0x24+12]=b"FIXTURE_DSR\0"
tpf.extend(dds)
bnd=bytearray(0x80)
bnd[:10]=b"BND307D7R6"
struct.pack_into("<I",bnd,0x10,1)
struct.pack_into("<IIIIII",bnd,0x20,0x40,len(tpf),0x80,42,0x38,len(tpf))
name=b"C:\\DSR\\fixture.tpf\0"
bnd[0x38:0x38+len(name)]=name
bnd.extend(tpf)
target=hashlib.sha256(top).hexdigest()
ok=report()
match.scan_blob(bytes(bnd),"synthetic.bnd",target,ok)
assert ok["containers"]==1 and ok["bc1_candidates"]==1 and len(ok["matches"])==1,ok
assert ok["matches"][0]["texture"]=="FIXTURE_DSR"
bad=report()
match.scan_blob(bytes(bnd),"synthetic.bnd","0"*64,bad)
assert bad["bc1_candidates"]==1 and not bad["matches"],bad
print("V230_BC1_MIP0_EXACT_TPF_BND3_POSITIVE_AND_NEGATIVE_PASS authority=NONE")
