#!/usr/bin/env python3
"""Read-only, SHA-pinned original DSR legacy GI EnvDiffuse / EnvSpec identity RE."""
import hashlib, pathlib, sys
if len(sys.argv)!=2: sys.exit("Pass exact retail DarkSoulsRemastered.exe")
b=pathlib.Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=="a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b"
def read(va,n):
    if 0x140001000<=va<0x14129c400: off=0x400+va-0x140001000
    elif 0x14129d000<=va<0x141a24c00: off=0x129b800+va-0x14129d000
    else: raise AssertionError(hex(va))
    return b[off:off+n]
def exact(va,h):
    wanted=bytes.fromhex(h)
    assert read(va,len(wanted))==wanted,hex(va)
def txt(va):
    return read(va,160).decode("utf-16le","ignore").split("\x00")[0]
exact(0x140563ea2,"44 0f bf 49 50") # EnvDif legacy record ID +0x50
exact(0x140563f32,"44 0f bf 49 52") # EnvSpc legacy record ID +0x52
assert txt(0x1412e9038)=="EnvDif_%s_%03d"
assert txt(0x1412e9018)=="EnvSpc_%s_%03d"
for va,slot in ((0x1401eb0a3,11),(0x1401eb0c0,13),(0x1401eb25a,12),(0x1401eb277,14)):
    exact(va,"ba "+slot.to_bytes(4,"little").hex(" "))
for va,target in ((0x1401eaf68,0x140563e70),(0x1401eb11e,0x140563f00)):
    op=read(va,5)
    assert op[0]==0xe8 and va+5+int.from_bytes(op[1:],"little",signed=True)==target
print("LEGACY_GI_SOURCE_SPLIT_RE_PASS: EnvDif+0x50 vs EnvSpc+0x52, slots 11/13 vs 12/14")
print("ACTUAL_VALUE_DIFFERENCE=OPEN, ON_DRAW_ROUTING=OPEN, PIXELS=OPEN")
