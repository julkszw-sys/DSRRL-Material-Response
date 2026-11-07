#!/usr/bin/env python3
"""V446 self-contained minimal static witness: retail EXE, no modification.
Full 54-instruction/70-mutation verifier is in V446 evidence handoff archive.
"""
import hashlib,struct,sys
from pathlib import Path
EXPECTED_SHA="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
BASE=0x140000000
SIG={
0x21f600:"440fbe8192000000",
0x21f608:"0fbe8193000000",
0x21ca5b:"40f6c7e0",
0x222ad2:"ba4c800000",
0x2efa79:"0fbe8193000000",
0x2efa84:"83898400000020",
0x2efa8b:"889193000000",
0x2fa36c:"0fbe8293000000",
0x2fa378:"838a8400000020",
0x2fa37f:"44888293000000",
0x326368:"0fbe8193000000",
0x326373:"83898400000020",
0x32637a:"889193000000",
0x326c17:"0fbe8a93000000",
0x326c22:"838a8400000020",
0x326c29:"888293000000",
0x326f83:"0fbe8a93000000",
0x326f8e:"838a8400000020",
0x326f95:"888293000000",
0x3270d2:"0fbe8193000000",
0x3270dd:"83898400000020",
0x3270e4:"889193000000",
0x222ccc:"e9af49fbff",
0x219c3c:"e93fdafbff",
0x22024c:"488d8f10010000",
0x216414:"488d8f10010000"}
def sections(buf):
    pe=struct.unpack_from("<I",buf,0x3c)[0]
    assert buf[:2]==b"MZ" and buf[pe:pe+4]==b"PE\\0\\0"
    count=struct.unpack_from("<H",buf,pe+6)[0]
    optional=struct.unpack_from("<H",buf,pe+20)[0]
    off=pe+24+optional
    for _ in range(count):
        name=buf[off:off+8].rstrip(b"\\0").decode("latin1")
        virtual_size,virtual_addr,raw_size,raw_offset=struct.unpack_from("<IIII",buf,off+8)
        yield (name,virtual_addr,max(virtual_size,raw_size),raw_offset)
        off+=40
def check(path):
    raw=Path(path).read_bytes()
    assert hashlib.sha256(raw).hexdigest()==EXPECTED_SHA,"wrong retail EXE"
    sec=list(sections(raw))
    def get(rva,count):
        for name,start,size,offset in sec:
            if start<=rva and rva+count<=start+size:
                return raw[offset+rva-start:offset+rva-start+count]
        raise AssertionError(f"invalid RVA {rva:x}")
    for rva,op in SIG.items():
        assert get(rva,len(bytes.fromhex(op)))==bytes.fromhex(op),f"opcode mismatch: {rva:x}"
    def rel(rva,opcode):
        b=get(rva,5);assert b[0]==opcode
        return rva+5+struct.unpack("<i",b[1:5])[0]
    for site,dst,opcode in ((0x21cac7,0x222ac0,0xE8),
      (0x22346c,0x222b60,0xE8),(0x222bbb,0x1d7580,0xE8),
      (0x21a648,0x219ad0,0xE8),(0x219b2b,0x1d7580,0xE8),
      (0x222ccc,0x1d7680,0xE9),(0x219c3c,0x1d7680,0xE9)):
        assert rel(site,opcode)==dst,f"call/jump mismatch: {site:x}"
    assert struct.unpack("<Q",get(0x12ec858,8))[0]==BASE+0x21c970
    print(f"V446 QUICK STATIC PASS: {len(SIG)} exact signatures, 7 typed edges, FrpgModel vtable; runtime/pixel OPEN")
if __name__=="__main__":
    if len(sys.argv)!=2:raise SystemExit("Usage: python verify_v446_quick.py DarkSoulsRemastered.exe")
    check(sys.argv[1])
