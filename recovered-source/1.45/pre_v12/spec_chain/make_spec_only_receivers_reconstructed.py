#!/usr/bin/env python3
from pathlib import Path
import struct, hashlib, math
BASE=Path('/mnt/data/spec_chain/FULL24_REBUILT.addon64')
OUTDIR=Path('/mnt/data/full24_work'); OUTDIR.mkdir(parents=True,exist_ok=True)
REC_OFFSETS=[0x100070,0x100080,0x100090]
NAMES=['FRPG_Phn_DifSpcBmp______Csd_HemEnv_SPEC_ONLY.fpo','FRPG_Phn_DifSpcBmp______Sdw_HemEnv_SPEC_ONLY.fpo','FRPG_Phn_DifSpcBmp__________HemEnv_SPEC_ONLY.fpo']
EXPECTED_BASE='cb95e2332c2d2dfd2184fe8ea04a882ea6f5ff4ea314e421e7eedcf768d5609d'
EXPECTED_CUR=['a4e5a097dcae88c7fd0897ae82c5842ef40d01b274f97d043c1937b4d28e4226','1c028ba78bc934c1887bf041ecb7d884029be7744afe97a75d6fab3d52fbe496','702140b61b38cd22ade40737f49de7eee8cdb4a8e849cbc156eef9bfbdbb72b2']
EXPECTED_OUT=['4ab7570bac9a7ef9c4dbb67e001deba2cfb6761ab901f80f950a7224dc9df959','fe2efca508f44b7a30f4656ffa0548f66e2e402bc3612f603fe8b6b029a4f3b5','2075374f6eaa88d5021368b8626cc53ff7573d9acaf24610f573d5a50756d73d']
# SHEX dword operand patches recovered from the historical SPEC_ONLY payloads.
# They implement only the documented diffuse/c100 rollback:
# cb12[1] -> stock cb0[9] at two diffuse uses, and pow exponent 1.0 -> 2.2 RGB.
PATCHES=[
 [(1203,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(1293,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(1307,[0x3f800000]*3,[0x400ccccd]*3)],
 [(1112,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(1202,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(1216,[0x3f800000]*3,[0x400ccccd]*3)],
 [(772,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(862,[0x0000000c,0x00000001],[0x00000000,0x00000009]),(876,[0x3f800000]*3,[0x400ccccd]*3)],
]
MASK=0xffffffff
SROT=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
K=[int(abs(math.sin(i+1))*(1<<32)) & MASK for i in range(64)]
def rol(x,n):return ((x<<n)|(x>>(32-n)))&MASK
def transform(state,block):
 a,b,c,d=state;M=list(struct.unpack('<16I',block));A,B,C,D=a,b,c,d
 for i in range(64):
  if i<16:F=(B&C)|((~B)&D);g=i
  elif i<32:F=(D&B)|((~D)&C);g=(5*i+1)%16
  elif i<48:F=B^C^D;g=(3*i+5)%16
  else:F=C^(B|(~D));g=(7*i)%16
  F=(F+A+K[i]+M[g])&MASK;A,D,C,B=D,C,B,(B+rol(F,SROT[i]))&MASK
 return [(a+A)&MASK,(b+B)&MASK,(c+C)&MASK,(d+D)&MASK]
def checksum(data):
 p=data[0x14:];size=len(p);nbits=size*8;state=[0x67452301,0xefcdab89,0x98badcfe,0x10325476];full=size&~63
 for o in range(0,full,64):state=transform(state,p[o:o+64])
 rem=p[full:]
 if len(rem)>=56:
  state=transform(state,rem+b'\x80'+b'\0'*(63-len(rem)));M=[0]*16;M[0]=nbits;M[15]=(nbits>>2)|1;state=transform(state,struct.pack('<16I',*M))
 else:
  buf=bytearray(64);struct.pack_into('<I',buf,0,nbits);buf[4:4+len(rem)]=rem;buf[4+len(rem)]=0x80;struct.pack_into('<I',buf,60,(nbits>>2)|1);state=transform(state,bytes(buf))
 return struct.pack('<4I',*state)
def sha(b):return hashlib.sha256(b).hexdigest()
def pe(b):
 p=struct.unpack_from('<I',b,0x3c)[0];op=p+24;img=struct.unpack_from('<Q',b,op+24)[0];n=struct.unpack_from('<H',b,p+6)[0];os=struct.unpack_from('<H',b,p+20)[0];sh=op+os;secs=[]
 for i in range(n):
  o=sh+i*40;vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8);secs.append((vs,va,rs,ro))
 return img,secs
def vaoff(b,av):
 img,secs=pe(b);r=av-img
 for vs,va,rs,ro in secs:
  if va<=r<va+max(vs,rs):return ro+r-va
 raise KeyError(hex(av))
def shex(b):
 n=struct.unpack_from('<I',b,28)[0]
 for o in struct.unpack_from('<%dI'%n,b,32):
  if b[o:o+4] in (b'SHEX',b'SHDR'):
   sz=struct.unpack_from('<I',b,o+4)[0];return o+8,sz
 raise RuntimeError('no SHEX')
b=BASE.read_bytes();assert sha(b)==EXPECTED_BASE
for i,(rec,name,patches) in enumerate(zip(REC_OFFSETS,NAMES,PATCHES)):
 av,sz=struct.unpack_from('<QI',b,rec);fo=vaoff(b,av);d=bytearray(b[fo:fo+sz]);assert sha(d)==EXPECTED_CUR[i]
 so,ss=shex(d);words=list(struct.unpack_from('<%dI'%(ss//4),d,so))
 for pos,old,new in patches:
  assert words[pos:pos+len(old)]==old,(i,pos,words[pos:pos+len(old)],old)
  words[pos:pos+len(new)]=new
 struct.pack_into('<%dI'%len(words),d,so,*words);d[4:20]=b'\0'*16;d[4:20]=checksum(bytes(d));assert sha(d)==EXPECTED_OUT[i],(i,sha(d))
 (OUTDIR/name).write_bytes(d);print(name,sha(d),len(d),'PASS')
