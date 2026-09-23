#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct, re, importlib.util
ROOT=Path(__file__).resolve().parent
BASE=ROOT/'base_v12.addon64'
PACK=ROOT/'PTDE_GI_ENVSPEC_PACK_RGBA.bin'
ASM=ROOT/'envcube_v15_1_lifted.S'
RDATA_PY=ROOT/'envcube_v15_1_rdata.py'
OUT=ROOT/'v151_from_lifted_source.addon64'
EXPECTED_BASE='3db7ad4a07c293d3b5a6579193c086bf96a7062f036d9ffb85efdb0400b00e79'
EXPECTED_TEXT='f8b474277787f9ae81cffc0c64f5586532bb804366accc3cbc8d877ea44852ac'
EXPECTED_RDATA='5c7ceaee061d03218923b630ba984b3f1c23350dd861dc4072cab6871e468e7e'
EXPECTED_PACK='c16c3fd75bcf34f3cc075da6da1ad10c9440ee4a3ca580fe7f74d07a2ce4eac3'
EXPECTED_OUT='7f10f908d598f8d6b952e2c78bce6eb97a8b84a297f956a069c439b7977b65cc'
def sha(b): return hashlib.sha256(b).hexdigest()
def require(b,h,label):
    if sha(b)!=h: raise SystemExit(f'{label}: {sha(b)} != {h}')
def parse_byte_asm(path):
    out=bytearray()
    for raw in path.read_text().splitlines():
        line=raw.split('#',1)[0].strip()
        if not line.startswith('.byte'): continue
        for tok in line[5:].split(','):
            tok=tok.strip()
            if tok: out.append(int(tok,0))
    return bytes(out)
def load_rdata():
    spec=importlib.util.spec_from_file_location('v151_rdata',RDATA_PY)
    mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    return mod.build_rdata()
def patch(buf,off,old,new,label):
    got=bytes(buf[off:off+len(old)])
    if got!=old: raise SystemExit(f'{label}: guard mismatch at {hex(off)}')
    buf[off:off+len(new)]=new
base=bytearray(BASE.read_bytes()); require(base,EXPECTED_BASE,'base V12')
text=parse_byte_asm(ASM); require(text,EXPECTED_TEXT,'lifted text')
rdata=load_rdata(); require(rdata,EXPECTED_RDATA,'declarative rdata')
pack=PACK.read_bytes(); require(pack,EXPECTED_PACK,'PackedGI')
if len(text)!=0x6000 or len(rdata)!=0x2000 or len(pack)!=0x2010000: raise SystemExit('component size')
patch(base,0x150,struct.pack('<I',0x001B7000),struct.pack('<I',0x021CF000),'SizeOfImage')
patch(base,0x3c8,struct.pack('<I',0x00082188),struct.pack('<I',0x0209B000),'.srgbmt VirtualSize')
patch(base,0x3d0,struct.pack('<I',0x00082200),struct.pack('<I',0x0209B000),'.srgbmt SizeOfRawData')
patch(base,0x8080,bytes.fromhex('e97b231000'),bytes.fromhex('e9cb011b00'),'init wrapper JMP')
patch(base,0x8660,bytes.fromhex('e91b1e1000'),bytes.fromhex('e98bff1a00'),'uninit wrapper JMP')
patch(base,0x19546f,bytes.fromhex('e89cf3ffff'),bytes.fromhex('e84cb40100'),'PRE')
patch(base,0x195484,bytes.fromhex('e807ffffff'),bytes.fromhex('e887cd0100'),'POST')
patch(base,0x1954f7,bytes.fromhex('e874eaffff'),bytes.fromhex('e8f4b20100'),'PREPARE')
if len(base)!=0x1AF600: raise SystemExit(hex(len(base)))
base += b'\0'*(0x1B0400-len(base)); base += text; base += rdata; base += pack
require(base,EXPECTED_OUT,'V15.1 rebuilt from lifted source')
OUT.write_bytes(base)
print('EXACT V15.1 PASS',EXPECTED_OUT)
