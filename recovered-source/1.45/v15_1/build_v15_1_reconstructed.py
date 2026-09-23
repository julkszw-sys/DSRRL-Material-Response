from pathlib import Path
import hashlib, struct

ROOT=Path(__file__).resolve().parent
BASE=ROOT/'base_v12.addon64'
EXT_TEXT=ROOT/'v15_1_ext_text.hex'
EXT_RDATA=ROOT/'v15_1_ext_rdata.hex'
PACK=ROOT/'PTDE_GI_ENVSPEC_PACK_RGBA.bin'
OUT=ROOT/'v151_reconstructed.addon64'

EXPECTED_BASE='3db7ad4a07c293d3b5a6579193c086bf96a7062f036d9ffb85efdb0400b00e79'
EXPECTED_EXT_TEXT='f8b474277787f9ae81cffc0c64f5586532bb804366accc3cbc8d877ea44852ac'
EXPECTED_EXT_RDATA='5c7ceaee061d03218923b630ba984b3f1c23350dd861dc4072cab6871e468e7e'
EXPECTED_PACK='c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3'
EXPECTED_OUT='7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc'

def sha(b): return hashlib.sha256(b).hexdigest()
def require(b,h,label):
    got=sha(b)
    if got!=h: raise SystemExit(f'{label} sha mismatch {got} != {h}')

def patch(buf,off,old,new,label):
    got=bytes(buf[off:off+len(old)])
    if got!=old: raise SystemExit(f'{label}: expected {old.hex()} got {got.hex()} at {hex(off)}')
    buf[off:off+len(new)]=new

base=bytearray(BASE.read_bytes()); require(base,EXPECTED_BASE,'base V12')
ext_text=bytes.fromhex(EXT_TEXT.read_text()); require(ext_text,EXPECTED_EXT_TEXT,'ext_text')
ext_rdata=bytes.fromhex(EXT_RDATA.read_text()); require(ext_rdata,EXPECTED_EXT_RDATA,'ext_rdata')
pack=PACK.read_bytes(); require(pack,EXPECTED_PACK,'pack')
if len(ext_text)!=0x6000 or len(ext_rdata)!=0x2000 or len(pack)!=0x2010000:
    raise SystemExit('component size mismatch')

# PE image/section growth: .srgbmt 0x82188/0x82200 -> 0x209B000; image -> 0x21CF000.
patch(base,0x150,struct.pack('<I',0x001B7000),struct.pack('<I',0x021CF000),'SizeOfImage')
patch(base,0x3c8,struct.pack('<I',0x00082188),struct.pack('<I',0x0209B000),'.srgbmt VirtualSize')
patch(base,0x3d0,struct.pack('<I',0x00082200),struct.pack('<I',0x0209B000),'.srgbmt SizeOfRawData')

# Existing host entry stubs -> new EnvSpec init/uninit wrappers.
patch(base,0x8080,bytes.fromhex('e97b231000'),bytes.fromhex('e9cb011b00'),'init wrapper JMP')
patch(base,0x8660,bytes.fromhex('e91b1e1000'),bytes.fromhex('e98bff1a00'),'uninit wrapper JMP')

# Existing V12 draw callsites -> EnvSpec PRE / POST / PREPARE wrappers.
patch(base,0x19546f,bytes.fromhex('e89cf3ffff'),bytes.fromhex('e84cb40100'),'PRE call')
patch(base,0x195484,bytes.fromhex('e807ffffff'),bytes.fromhex('e887cd0100'),'POST call')
patch(base,0x1954f7,bytes.fromhex('e874eaffff'),bytes.fromhex('e8f4b20100'),'PREPARE call')

# V12 .srgbmt ends at file 0x1AF600. Align extension text to audited RVA 0x1B7000 => file 0x1B0400.
if len(base)!=0x1AF600: raise SystemExit(f'unexpected V12 file size {hex(len(base))}')
base += b'\0' * (0x1B0400-len(base))
base += ext_text       # RVA 0x1B7000 .. 0x1BCFFF
base += ext_rdata      # RVA 0x1BD000 .. 0x1BEFFF
base += pack           # RVA 0x1BF000 .. 0x21CEFFF
if len(base)!=0x21C8400: raise SystemExit(f'unexpected output size {hex(len(base))}')
require(base,EXPECTED_OUT,'V15.1 output')
OUT.write_bytes(base)
print(EXPECTED_OUT)