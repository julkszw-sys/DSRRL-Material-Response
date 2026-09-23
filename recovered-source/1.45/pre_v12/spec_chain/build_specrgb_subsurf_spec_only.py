from pathlib import Path
import struct, hashlib, json, zipfile, math, subprocess

BASE=0x180000000
SRC=Path('/mnt/data/DSRRL_Material_Response_1.3_PTDE_SPEC_EQUIPMENT_SPEC_ONLY_RC.addon64')
EXPECTED_SRC_SHA='5794623c639017a7c0f75cdfece80810c45576996dc788abfb5b05e07c914915'
OUT=Path('/mnt/data/DSRRL_Material_Response_1.3_PTDE_SPEC_EQUIPMENT_SUBSURF_SPEC_ONLY_RC.addon64')
AUD=Path('/mnt/data/DSRRL_PTDE_SPEC_EQUIPMENT_SUBSURF_SPEC_ONLY_AUDIT.json')
README=Path('/mnt/data/DSRRL_PTDE_SPEC_EQUIPMENT_SUBSURF_SPEC_ONLY_README.txt')
PKG=Path('/mnt/data/DSRRL_MATERIAL_RESPONSE_1.3_PTDE_SPEC_EQUIPMENT_SUBSURF_SPEC_ONLY_RC.zip')
BUILDER=Path(__file__)
PREAUD=Path('/mnt/data/subsurf_spec_only_receiver_prebuild_audit.json')
TRANSFORMER=Path('/mnt/data/make_subsurf_spec_only_receivers.py')
WORK=Path('/mnt/data/full24_work')
SUB_PAYLOADS=[
 WORK/'FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf_SPEC_ONLY_T13.fpo',
 WORK/'FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf_SPEC_ONLY_T13.fpo',
 WORK/'FRPG_Phn_DifSpcBmp__________HemEnvSubsurf_SPEC_ONLY_T13.fpo',
]
SUB_EXPECTED=[
 '48c035ad2b2fdc8abf80072665214b51e130b073765c6b42c8e78ab37cd69e42',
 '071044a2c28b155c5152eff62a81d888ae8cad90fa2693a8ec9460193370d490',
 '90a3df9e658d1d8a77df5aed47ca9693e4d6ffe896f63baf39d52dd107c2d75b',
]
SUB_STOCK_HASHES=[
 '0b8288d686c8f349ad87352946be51ffd007462f25357326bf47e736e690e511',
 '885337e50f3d29f086fd18e1f7524f28712031d0264964aef5f37037df7d7bcb',
 '3002cfb9aee6835412399c3be267ab94c5706d7d5030fc4cafc82bc54c55a860',
]
SUB_NAMES=[
 'FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo',
 'FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo',
 'FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo',
]
DSBT_HASH='2706080f2b306245b90bf9d3fdfa38624b2ed0452a9b1aff830cf59de69b37d4'
PTDE_BODY_DONOR_HASH='af2f108831b783a43b0e02f047919719d14f38e68d6c5a97b80678d593ba1c95'
OLD_SLOT26_HASH='40afb3a429e821af595ff68f3f205109d2f2fdb38b6d404bcbb8f2d9720275ee'

# Current binary ABI
REG_RVA=0x81e80; REG_STRIDE=0x48; DONOR_TARGET=26; DONOR_CLONE=232; DONOR_FLAG_OFF=0x44
OLD_META_RVA=0x7fd80; META_STRIDE=0x38; OLD_META_COUNT=48
DEVICE_RVA=0x107880; GUARD_DISPATCH_RVA=0xe548
SPEC_OBJ_RVA=0x112310; SUB_OBJ_RVA=0x112330
T13_STATE_RVA=0x112350; T13_STATE_COUNT=64; T13_STATE_STRIDE=0x18
C100_ARRAY_RVA=0x107888; C101_ARRAY_RVA=0x107948
OLD_SHARED_GATE_RVA=0x10b154
T10_PREP_RVA=0x11350c; T10_RESTORE_RVA=0x113b45
OLD_PRE_WRAPPER_RVA=0x1130f0; OLD_POST_WRAPPER_RVA=0x113110
# Existing spec-only payloads already present in base
SPEC_PAYLOAD_RVAS=[0x17a400,0x17f1a0,0x183e20]
SPEC_PAYLOAD_SIZES=[19872,19572,18076]
# Patch sites
CREATE_PATCH_RVA=0x697b; CREATE_RETURN_RVA=0x6982
SELECT_PATCH_RVA=0x63fc; SELECT_PATCH_LEN=0x20; SELECT_RETURN_RVA=0x641c
CLEAN_PATCH_RVA=0x7e71; CLEAN_RETURN_RVA=0x7e78
GATE_CALL_RVA=0x632e
PRE_CALL_RVA=0x65e7; POST_CALL_RVA=0x666a
INIT_META_LEA_RVA=0x60dd; INIT_LIMIT_RVA=0x6118
DRAW_BOUND_RVA=0x62fd; DRAW_META_LEA_RVA=0x633d
DEVICE_META_LEA_RVA=0x6885; DEVICE_COUNT_RVA=0x688c
# New region appended to final .srgbmt section
CREATE_RVA=0x189000
SELECT_RVA=0x189300
CLEAN_RVA=0x189500
GATE_RVA=0x189700
PRE_RVA=0x189900
POST_RVA=0x189c00
META_RVA=0x189e00
STRINGS_RVA=0x18aa00
PAYLOAD0_RVA=0x18b000


def align(x,a): return (x+a-1)&~(a-1)
def parse_pe(b):
 pe=struct.unpack_from('<I',b,0x3c)[0]; n=struct.unpack_from('<H',b,pe+6)[0]; osz=struct.unpack_from('<H',b,pe+20)[0]; opt=pe+24; st=opt+osz; secs=[]
 for i in range(n):
  o=st+i*40; name=b[o:o+8].rstrip(b'\0').decode('ascii'); vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8); ch=struct.unpack_from('<I',b,o+36)[0]
  secs.append(dict(i=i,off=o,name=name,vs=vs,va=va,rs=rs,ro=ro,ch=ch))
 return pe,opt,st,secs
def rva2off(secs,rva):
 for s in secs:
  if s['ro'] and s['va']<=rva<s['va']+s['rs']: return s['ro']+(rva-s['va'])
 raise ValueError(hex(rva))
def rel32(src_next,target): return struct.pack('<i',target-src_next)
def ripdisp(next_rva,target): return rel32(next_rva,target)
class Asm:
 def __init__(self,rva): self.rva=rva; self.b=bytearray(); self.labels={}; self.fix=[]
 def pos(self): return self.rva+len(self.b)
 def raw(self,x): self.b.extend(x)
 def label(self,n): self.labels[n]=self.pos()
 def jmp_label(self,n): self.raw(b'\xe9'); at=len(self.b); self.raw(b'\0'*4); self.fix.append((at,n))
 def jcc(self,cc,n): self.raw(b'\x0f'+bytes([cc])); at=len(self.b); self.raw(b'\0'*4); self.fix.append((at,n))
 def jmp_abs(self,t): self.raw(b'\xe9'+rel32(self.pos()+5,t))
 def call_abs(self,t): self.raw(b'\xe8'+rel32(self.pos()+5,t))
 def finish(self):
  for at,n in self.fix: self.b[at:at+4]=rel32(self.rva+at+4,self.labels[n])
  return bytes(self.b)

# DXBC checksum
MASK=0xffffffff; SROT=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4; K=[int(abs(math.sin(i+1))*(1<<32))&MASK for i in range(64)]
def rol(x,n): return ((x<<n)|(x>>(32-n)))&MASK
def transform(state,block):
 a,b,c,d=state; M=list(struct.unpack('<16I',block)); A,B,C,D=a,b,c,d
 for i in range(64):
  if i<16:F=(B&C)|((~B)&D);g=i
  elif i<32:F=(D&B)|((~D)&C);g=(5*i+1)%16
  elif i<48:F=B^C^D;g=(3*i+5)%16
  else:F=C^(B|(~D));g=(7*i)%16
  F=(F+A+K[i]+M[g])&MASK; A,D,C,B=D,C,B,(B+rol(F,SROT[i]))&MASK
 return [(a+A)&MASK,(b+B)&MASK,(c+C)&MASK,(d+D)&MASK]
def dxbc_checksum(data):
 p=data[0x14:]; size=len(p); nbits=size*8; state=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]; full=size&~63
 for o in range(0,full,64): state=transform(state,p[o:o+64])
 rem=p[full:]
 if len(rem)>=56:
  block=rem+b'\x80'+b'\0'*(63-len(rem)); state=transform(state,block); M=[0]*16; M[0]=nbits; M[15]=(nbits>>2)|1; state=transform(state,struct.pack('<16I',*M))
 else:
  buf=bytearray(64); struct.pack_into('<I',buf,0,nbits); buf[4:4+len(rem)]=rem; buf[4+len(rem)]=0x80; struct.pack_into('<I',buf,60,(nbits>>2)|1); state=transform(state,bytes(buf))
 return struct.pack('<4I',*state)

# code generators
def build_create(all_rvas,all_sizes):
 a=Asm(CREATE_RVA); a.raw(b'\x31\xc0')
 for base in (SPEC_OBJ_RVA,SUB_OBJ_RVA):
  for i in range(3):
   t=base+8*i; s=a.pos(); a.raw(b'\x48\x89\x05'+ripdisp(s+7,t))
 # create six objects; current device ptr is stable global
 for i,(pr,sz) in enumerate(zip(all_rvas,all_sizes)):
  s=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(s+7,DEVICE_RVA)); a.raw(b'\x48\x85\xc9'); a.jcc(0x84,'done')
  a.raw(b'\x48\x8b\x01')
  s=a.pos(); a.raw(b'\x48\x8d\x15'+ripdisp(s+7,pr)); a.raw(b'\x41\xb8'+struct.pack('<I',sz)); a.raw(b'\x45\x31\xc9')
  target=(SPEC_OBJ_RVA if i<3 else SUB_OBJ_RVA)+8*(i%3); s=a.pos(); a.raw(b'\x4c\x8d\x15'+ripdisp(s+7,target)); a.raw(b'\x4c\x89\x54\x24\x20')
  a.raw(b'\x48\x8b\x40\x78'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 a.label('done'); a.raw(bytes.fromhex('49 8d 85 58 0f 10 00')); a.jmp_abs(CREATE_RETURN_RVA); return a.finish()

def build_selector(meta_views):
 a=Asm(SELECT_RVA)
 # route 2: existing ordinary SPEC_ONLY
 a.raw(b'\x41\x80\xfc\x02'); a.jcc(0x85,'check3')
 a.raw(b'\x48\x63\x06'); a.raw(b'\x83\xf8\x09'); a.jcc(0x82,'null'); a.raw(b'\x83\xf8\x0b'); a.jcc(0x87,'null'); a.raw(b'\x83\xe8\x09')
 s=a.pos(); a.raw(b'\x48\x8d\x0d'+ripdisp(s+7,SPEC_OBJ_RVA)); a.raw(b'\x48\x8b\x0c\xc1'); a.raw(bytes.fromhex('48 89 4d 87')); a.jmp_abs(SELECT_RETURN_RVA)
 a.label('check3'); a.raw(b'\x41\x80\xfc\x03'); a.jcc(0x85,'old')
 # exact metadata identity, not family resemblance
 for i,v in enumerate(meta_views):
  a.raw(b'\x48\xb8'+struct.pack('<Q',BASE+v)); a.raw(b'\x48\x39\xc6'); a.jcc(0x84,f'sub{i}')
 a.jmp_label('null')
 for i in range(3):
  a.label(f'sub{i}'); s=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(s+7,SUB_OBJ_RVA+8*i)); a.raw(bytes.fromhex('48 89 4d 87')); a.jmp_abs(SELECT_RETURN_RVA)
 a.label('old')
 s=a.pos(); a.raw(b'\x48\x8d\x05'+ripdisp(s+7,C100_ARRAY_RVA)); s=a.pos(); a.raw(b'\x48\x8d\x0d'+ripdisp(s+7,C101_ARRAY_RVA)); a.raw(bytes.fromhex('45 84 e4 48 0f 44 c8 48 63 06 48 8b 0c c1 48 89 4d 87')); a.jmp_abs(SELECT_RETURN_RVA)
 a.label('null'); a.raw(b'\x31\xc9'+bytes.fromhex('48 89 4d 87')); a.jmp_abs(SELECT_RETURN_RVA); return a.finish()

def build_cleanup():
 a=Asm(CLEAN_RVA)
 for base in (SPEC_OBJ_RVA,SUB_OBJ_RVA):
  for i in range(3):
   t=base+8*i; s=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(s+7,t)); a.raw(b'\x48\x85\xc9'); a.jcc(0x84,f'n{base:x}_{i}'); a.raw(b'\x48\x8b\x01\x48\x8b\x40\x10'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA)); a.raw(b'\x31\xc0'); s=a.pos(); a.raw(b'\x48\x89\x05'+ripdisp(s+7,t)); a.label(f'n{base:x}_{i}')
 s=a.pos(); a.raw(b'\x48\x8b\x3d'+ripdisp(s+7,0x107a10)); a.jmp_abs(CLEAN_RETURN_RVA); return a.finish()

def build_gate(meta_offsets):
 a=Asm(GATE_RVA); a.raw(b'\x83\xff\x1a'); a.jcc(0x85,'old')
 # exact appended Subsurf metadata only
 for j,x in enumerate(meta_offsets):
  a.raw(b'\x48\x81\xfa'+struct.pack('<I',x)); a.jcc(0x84,'identity_ok')
 a.raw(b'\x31\xc0\xc3')
 a.label('identity_ok')
 # Preserve caller RCX/RDX, use old certified companion gate by temporarily using shared donor 23.
 a.raw(b'\x51\x52\x48\x83\xec\x28'); a.raw(b'\xbf\x17\x00\x00\x00'); a.raw(b'\x48\x8b\x4c\x24\x30'); a.raw(b'\x48\x8b\x54\x24\x28'); a.call_abs(OLD_SHARED_GATE_RVA); a.raw(b'\xbf\x1a\x00\x00\x00'); a.raw(b'\x84\xc0'); a.jcc(0x84,'cleanup_false')
 # Allocate/find per-native-context t13 state now, so capacity failure fails open before selecting custom shader.
 a.raw(b'\x48\x8b\x4c\x24\x30\x48\x8b\x01\xff\x10'); a.raw(b'\x48\x85\xc0'); a.jcc(0x84,'cleanup_false'); a.raw(b'\x49\x89\xc1') # r9=ctx
 s=a.pos(); a.raw(b'\x4c\x8d\x05'+ripdisp(s+7,T13_STATE_RVA)); a.raw(b'\x41\xba'+struct.pack('<I',T13_STATE_COUNT))
 a.label('scan'); a.raw(b'\x4d\x39\x08'); a.jcc(0x84,'cleanup_true'); a.raw(b'\x49\x83\x38\x00'); a.jcc(0x85,'next')
 a.raw(b'\x31\xc0\xf0\x4d\x0f\xb1\x08'); a.jcc(0x84,'cleanup_true')
 a.label('next'); a.raw(b'\x49\x83\xc0\x18\x41\xff\xca'); a.jcc(0x85,'scan')
 a.label('cleanup_false'); a.raw(b'\x31\xc0'); a.jmp_label('cleanup')
 a.label('cleanup_true'); a.raw(b'\xb0\x01')
 a.label('cleanup'); a.raw(b'\x48\x83\xc4\x28\x5a\x59\xc3')
 a.label('old'); a.jmp_abs(OLD_SHARED_GATE_RVA); return a.finish()

def build_pre():
 a=Asm(PRE_RVA); a.raw(b'\x41\x80\xfc\x03'); a.jcc(0x85,'old')
 a.raw(b'\x48\x83\xec\x68')
 # Existing certified t10 prepare+get+restore is used only as no-draw companion acquisition; t10 is restored before the draw.
 a.raw(b'\x48\x89\xd9'); a.call_abs(T10_PREP_RVA)
 a.raw(b'\x48\xc7\x44\x24\x20\x00\x00\x00\x00'); a.raw(b'\x48\x8b\x03\x48\x89\xd9\xba\x0a\x00\x00\x00\x41\xb8\x01\x00\x00\x00\x4c\x8d\x4c\x24\x20\x48\x8b\x80\x48\x02\x00\x00'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 a.raw(b'\x48\x89\xd9'); a.call_abs(T10_RESTORE_RVA)
 # Find preallocated state entry.
 s=a.pos(); a.raw(b'\x4c\x8d\x15'+ripdisp(s+7,T13_STATE_RVA)); a.raw(b'\x41\xbb'+struct.pack('<I',T13_STATE_COUNT)); a.label('find'); a.raw(b'\x49\x39\x1a'); a.jcc(0x84,'found'); a.raw(b'\x49\x83\xc2\x18\x41\xff\xcb'); a.jcc(0x85,'find'); a.jmp_label('finish')
 a.label('found'); a.raw(b'\x4c\x89\x54\x24\x30')
 # Save current t13.
 a.raw(b'\x48\xc7\x44\x24\x28\x00\x00\x00\x00'); a.raw(b'\x48\x8b\x03\x48\x89\xd9\xba\x0d\x00\x00\x00\x41\xb8\x01\x00\x00\x00\x4c\x8d\x4c\x24\x28\x48\x8b\x80\x48\x02\x00\x00'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 a.raw(b'\x4c\x8b\x54\x24\x30\x48\x8b\x44\x24\x28\x49\x89\x42\x08\x49\xc7\x42\x10\x01\x00\x00\x00')
 # Bind captured companion (or t1 fail-open selected by existing prepare) to t13.
 a.raw(b'\x48\x8b\x03\x48\x89\xd9\xba\x0d\x00\x00\x00\x41\xb8\x01\x00\x00\x00\x4c\x8d\x4c\x24\x20\x48\x8b\x40\x40'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 # Release PSGet slot10 temporary ref.
 a.raw(b'\x48\x8b\x4c\x24\x20\x48\x85\xc9'); a.jcc(0x84,'finish'); a.raw(b'\x48\x8b\x01\x48\x8b\x40\x10'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 a.label('finish'); a.raw(b'\x48\x83\xc4\x68\x48\x8b\x03\x45\x31\xc9\xc3')
 a.label('old'); a.jmp_abs(OLD_PRE_WRAPPER_RVA); return a.finish()

def build_post():
 a=Asm(POST_RVA); a.raw(b'\x41\x80\xfc\x03'); a.jcc(0x85,'old'); a.raw(b'\x48\x83\xec\x48')
 s=a.pos(); a.raw(b'\x4c\x8d\x15'+ripdisp(s+7,T13_STATE_RVA)); a.raw(b'\x41\xbb'+struct.pack('<I',T13_STATE_COUNT)); a.label('find'); a.raw(b'\x49\x39\x1a'); a.jcc(0x84,'found'); a.raw(b'\x49\x83\xc2\x18\x41\xff\xcb'); a.jcc(0x85,'find'); a.jmp_label('finish')
 a.label('found'); a.raw(b'\x49\x83\x7a\x10\x01'); a.jcc(0x85,'finish'); a.raw(b'\x4c\x89\x54\x24\x30\x49\x8b\x42\x08\x48\x89\x44\x24\x20')
 # Restore prior t13.
 a.raw(b'\x48\x8b\x03\x48\x89\xd9\xba\x0d\x00\x00\x00\x41\xb8\x01\x00\x00\x00\x4c\x8d\x4c\x24\x20\x48\x8b\x40\x40'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 # Release prior ref acquired by PSGet.
 a.raw(b'\x48\x8b\x4c\x24\x20\x48\x85\xc9'); a.jcc(0x84,'clear'); a.raw(b'\x48\x8b\x01\x48\x8b\x40\x10'); s=a.pos(); a.raw(b'\xff\x15'+ripdisp(s+6,GUARD_DISPATCH_RVA))
 a.label('clear'); a.raw(b'\x4c\x8b\x54\x24\x30\x49\xc7\x42\x08\x00\x00\x00\x00\x49\xc7\x42\x10\x00\x00\x00\x00')
 a.label('finish'); a.raw(b'\x48\x83\xc4\x48\x48\x8b\x03\x45\x31\xc9\xc3'); a.label('old'); a.jmp_abs(OLD_POST_WRAPPER_RVA); return a.finish()

# Load/verify base
src=SRC.read_bytes(); assert hashlib.sha256(src).hexdigest()==EXPECTED_SRC_SHA
b=bytearray(src); pe,opt,st,secs=parse_pe(b); sec=next(s for s in secs if s['name']=='.srgbmt'); file_align=struct.unpack_from('<I',b,opt+36)[0]; sect_align=struct.unpack_from('<I',b,opt+32)[0]
assert sec['ro']+sec['rs']==len(src); assert file_align==0x200 and sect_align==0x1000
# verify BSS planned ranges have no raw backing and no static refs are assumed by construction audit
assert T13_STATE_RVA+T13_STATE_COUNT*T13_STATE_STRIDE <= 0x113000
# verify new payloads
subdata=[]
for p,e in zip(SUB_PAYLOADS,SUB_EXPECTED):
 d=p.read_bytes(); assert hashlib.sha256(d).hexdigest()==e; assert d[:4]==b'DXBC' and d[4:20]==dxbc_checksum(d); subdata.append(d)
preaudit=json.loads(PREAUD.read_text())
assert isinstance(preaudit,list) and len(preaudit)==3 and all(x.get('checksum')=='PASS' and x.get('b12_decl')==1 and x.get('t10_decl')==1 and x.get('t13_decl')==1 and x.get('t14_decl')==1 and x.get('s14_decl')==1 and x.get('t13_sample')==1 and x.get('t10_samples')==1 and x.get('t14_samples')==1 and x.get('t9_samples')==0 and x.get('pow22_after_t13') is False and x.get('non_shex_byte_identical') for x in preaudit)
# payload placement
sub_rvas=[PAYLOAD0_RVA]
for d in subdata[:-1]: sub_rvas.append(align(sub_rvas[-1]+len(d),16))
# strings blob: 3 hashes, 3 names, donor hash. Keep exact byte lengths separately.
strings=[]; cursor=STRINGS_RVA; string_rvas={}
def addstr(key,s):
 global cursor
 bs=s.encode('ascii'); string_rvas[key]=(cursor,len(bs)); strings.append((cursor,bs+b'\0')); cursor+=len(bs)+1
for i,h in enumerate(SUB_STOCK_HASHES): addstr(f'h{i}',h)
for i,n in enumerate(SUB_NAMES): addstr(f'n{i}',n)
addstr('dsbt',DSBT_HASH)
assert cursor < PAYLOAD0_RVA
# relocated metadata 48+3
old_meta_off=rva2off(secs,OLD_META_RVA); old_meta=bytes(b[old_meta_off:old_meta_off+OLD_META_COUNT*META_STRIDE]); assert len(old_meta)==0xa80
meta=bytearray(old_meta)
for i,(h,n,fam) in enumerate(zip(SUB_STOCK_HASHES,SUB_NAMES,[9,10,11])):
 hp,hl=string_rvas[f'h{i}']; np,nl=string_rvas[f'n{i}']; rec=struct.pack('<QQQQQQQ',BASE+hp,hl,(fam<<32)|0,0,0,BASE+np,nl); assert len(rec)==META_STRIDE; meta.extend(rec)
assert len(meta)==51*META_STRIDE==0xb28
meta_views=[META_RVA+0x14+(48+i)*META_STRIDE for i in range(3)]; meta_offsets=[(48+i)*META_STRIDE for i in range(3)]
# build helpers
all_rvas=SPEC_PAYLOAD_RVAS+sub_rvas; all_sizes=SPEC_PAYLOAD_SIZES+[len(x) for x in subdata]
create=build_create(all_rvas,all_sizes); selector=build_selector(meta_views); cleanup=build_cleanup(); gate=build_gate(meta_offsets); pre=build_pre(); post=build_post()
assert len(create)<=0x300 and len(selector)<=0x200 and len(cleanup)<=0x200 and len(gate)<=0x200 and len(pre)<=0x300 and len(post)<=0x200, [len(x) for x in [create,selector,cleanup,gate,pre,post]]
# extend section
final_end=max(CREATE_RVA+len(create),SELECT_RVA+len(selector),CLEAN_RVA+len(cleanup),GATE_RVA+len(gate),PRE_RVA+len(pre),POST_RVA+len(post),META_RVA+len(meta),cursor,sub_rvas[-1]+len(subdata[-1]))
new_vs=final_end-sec['va']; new_rs=align(new_vs,file_align); new_file_end=sec['ro']+new_rs; b.extend(b'\0'*(new_file_end-len(b)))
struct.pack_into('<I',b,sec['off']+8,new_vs); struct.pack_into('<I',b,sec['off']+16,new_rs); struct.pack_into('<I',b,opt+56,align(sec['va']+new_vs,sect_align))
def newoff(rva): assert sec['va']<=rva<sec['va']+new_rs; return sec['ro']+(rva-sec['va'])
# write helpers/table/strings/payloads
for rva,data in [(CREATE_RVA,create),(SELECT_RVA,selector),(CLEAN_RVA,cleanup),(GATE_RVA,gate),(PRE_RVA,pre),(POST_RVA,post),(META_RVA,meta)]: b[newoff(rva):newoff(rva)+len(data)]=data
for rva,bs in strings: b[newoff(rva):newoff(rva)+len(bs)]=bs
for rva,d in zip(sub_rvas,subdata): b[newoff(rva):newoff(rva)+len(d)]=d
# donor registry slot26 clone exact PTDE Ps_Body donor232, then exact DSR DSBT hash ptr and route flag3
reg_off=rva2off(secs,REG_RVA); rec26=reg_off+DONOR_TARGET*REG_STRIDE; rec232=reg_off+DONOR_CLONE*REG_STRIDE
old26=bytes(b[rec26:rec26+REG_STRIDE]); clone=bytearray(b[rec232:rec232+REG_STRIDE])
# verify source donor hash pointer string
def read_va_str(ptr,n):
 rva=ptr-BASE; return bytes(b[rva2off(secs,rva):rva2off(secs,rva)+n]).decode('ascii')
ptr232,n232=struct.unpack_from('<QQ',b,rec232); assert read_va_str(ptr232,n232)==PTDE_BODY_DONOR_HASH
ptr26,n26=struct.unpack_from('<QQ',b,rec26); assert read_va_str(ptr26,n26)==OLD_SLOT26_HASH
struct.pack_into('<Q',clone,0,BASE+string_rvas['dsbt'][0]); struct.pack_into('<Q',clone,8,64); clone[DONOR_FLAG_OFF]=3; b[rec26:rec26+REG_STRIDE]=clone
# patch helper jumps at existing sites
# create jmp 7 bytes
o=rva2off(secs,CREATE_PATCH_RVA); assert b[o]==0xe9; b[o:o+7]=b'\xe9'+rel32(CREATE_PATCH_RVA+5,CREATE_RVA)+b'\x90\x90'
o=rva2off(secs,SELECT_PATCH_RVA); assert b[o]==0xe9; b[o:o+SELECT_PATCH_LEN]=b'\xe9'+rel32(SELECT_PATCH_RVA+5,SELECT_RVA)+b'\x90'*(SELECT_PATCH_LEN-5)
o=rva2off(secs,CLEAN_PATCH_RVA); assert b[o]==0xe9; b[o:o+7]=b'\xe9'+rel32(CLEAN_PATCH_RVA+5,CLEAN_RVA)+b'\x90\x90'
# call rel32 patches
def patch_call(rva,target,expected_old_target=None):
 o=rva2off(secs,rva); assert b[o]==0xe8; old=rva+5+struct.unpack_from('<i',b,o+1)[0]
 if expected_old_target is not None: assert old==expected_old_target,(hex(rva),hex(old),hex(expected_old_target))
 b[o+1:o+5]=rel32(rva+5,target); return old
patch_call(GATE_CALL_RVA,GATE_RVA,OLD_SHARED_GATE_RVA); patch_call(PRE_CALL_RVA,PRE_RVA,OLD_PRE_WRAPPER_RVA); patch_call(POST_CALL_RVA,POST_RVA,OLD_POST_WRAPPER_RVA)
# metadata-table/count patches. Validate old bytes first.
o=rva2off(secs,INIT_META_LEA_RVA); assert b[o:o+3]==bytes.fromhex('48 8d 1d'); b[o+3:o+7]=ripdisp(INIT_META_LEA_RVA+7,META_RVA)
o=rva2off(secs,INIT_LIMIT_RVA); assert b[o:o+3]==bytes.fromhex('48 81 fe') and struct.unpack_from('<I',b,o+3)[0]==0xa80; struct.pack_into('<I',b,o+3,0xb28)
o=rva2off(secs,DRAW_BOUND_RVA); assert b[o:o+2]==bytes.fromhex('83 f8') and b[o+2]==0x2f; b[o+2]=0x32
o=rva2off(secs,DRAW_META_LEA_RVA); assert b[o:o+4]==bytes.fromhex('49 8d b4 24'); struct.pack_into('<I',b,o+4,META_RVA+0x14)
o=rva2off(secs,DEVICE_META_LEA_RVA); assert b[o:o+3]==bytes.fromhex('48 8d 1d'); b[o+3:o+7]=ripdisp(DEVICE_META_LEA_RVA+7,META_RVA+0x14)
o=rva2off(secs,DEVICE_COUNT_RVA); assert b[o:o+2]==bytes.fromhex('41 bc') and struct.unpack_from('<I',b,o+2)[0]==0x30; struct.pack_into('<I',b,o+2,0x33)
# write candidate
OUT.write_bytes(b)
# static readback / invariants
fpe,fopt,fst,fsecs=parse_pe(b); fs=next(s for s in fsecs if s['name']=='.srgbmt'); assert fs['vs']==new_vs and fs['rs']==new_rs
# Existing .v13p producer bytes remain exactly unchanged.
v13p=next(s for s in secs if s['name']=='.v13p'); assert src[v13p['ro']:v13p['ro']+v13p['rs']]==bytes(b[v13p['ro']:v13p['ro']+v13p['rs']])
# Existing stock/ordinary receiver sections untouched; old portion of .srgbmt untouched except PE metadata not inside raw section.
for nm in ['.srgblt','.srgbml']:
 s=next(x for x in secs if x['name']==nm); assert src[s['ro']:s['ro']+s['rs']]==bytes(b[s['ro']:s['ro']+s['rs']])
# new payload identity/checksum
rb=[]
for i,(rva,d,e) in enumerate(zip(sub_rvas,subdata,SUB_EXPECTED)):
 got=bytes(b[newoff(rva):newoff(rva)+len(d)]); assert got==d and hashlib.sha256(got).hexdigest()==e and got[4:20]==dxbc_checksum(got)
 rb.append({'variant':['Csd','Sdw','plain'][i],'stock_hash':SUB_STOCK_HASHES[i],'payload_rva':hex(rva),'size':len(d),'sha256':e,'dxbc_checksum':'PASS','spec_slot':13,'stock_subsurf_slot_preserved':10,'b12_spec_island':True,'t14_envspec_branch':True,'post_t13_pow22_removed':True})
# donor slot readback: byte-identical clone except ptr/hash and flag; all response bytes from PTDE body donor retained.
new26=bytes(b[rec26:rec26+REG_STRIDE]); assert new26[8:DONOR_FLAG_OFF]==bytes(b[rec232+8:rec232+DONOR_FLAG_OFF]); assert new26[DONOR_FLAG_OFF+1:]==bytes(b[rec232+DONOR_FLAG_OFF+1:rec232+REG_STRIDE]); assert new26[DONOR_FLAG_OFF]==3
# PTDE body donor response vector is exact 1.0 and is preserved in route26 clone.
resp=struct.unpack_from('<6f',new26,0x20); assert all(abs(x-1.0)<1e-7 for x in resp),resp
# verify metadata exact
for i in range(3):
 ro=newoff(META_RVA+(48+i)*META_STRIDE); hp,hl,packed,payload,sz,np,nl=struct.unpack_from('<QQQQQQQ',b,ro); assert hl==64 and packed==((9+i)<<32) and payload==0 and sz==0
 assert bytes(b[newoff(hp-BASE):newoff(hp-BASE)+hl]).decode()==SUB_STOCK_HASHES[i]
# disasm for audit
subprocess.run(['llvm-objdump','-d','--x86-asm-syntax=intel',str(OUT)],stdout=open('/mnt/data/subsurf_spec_only_disasm.txt','w'),check=True)
# diff accounting prefix
def sec_for_off(off):
 if off<0x400:return 'PE_HEADERS'
 for s in secs:
  if s['ro'] and s['ro']<=off<s['ro']+s['rs']:return s['name']
 return 'OTHER'
changed=[i for i in range(len(src)) if src[i]!=b[i]]; sc={}
for x in changed: sc[sec_for_off(x)]=sc.get(sec_for_off(x),0)+1
# audit
outsha=hashlib.sha256(OUT.read_bytes()).hexdigest()
audit={
 'status':'PASS','date':'2026-09-19',
 'base':{'file':SRC.name,'sha256':EXPECTED_SRC_SHA,'semantic':'equipment SPEC_ONLY plain-route RC; existing DSRRL_PTDE_SPEC producer reused unchanged'},
 'candidate':{'file':OUT.name,'sha256':outsha,'size':OUT.stat().st_size,'build_key':'material_response_1_3_ptde_spec_equipment_subsurf_spec_only_v1'},
 'scope':{'material':'Ps_Body[DSBT].mtd','dsr_mtd_sha256':DSBT_HASH,'ptde_material':'Ps_Body[DSB].mtd','ptde_target_c101':1.0,'c101_policy':'PTDE Ps_Body[DSB] c101=1.0 consumed through b12[0] in transplanted operator-local spec island; donor record cloned from exact PTDE body route','equipment_textures':['BD_F_body_s','BD_M_body_s'],'ordinary_binding_occurrences':6},
 'producer':{'namespace':'DSRRL_PTDE_SPEC','new_loader':False,'new_sidecar_format':False,'v13p_byte_identical':True,'semantic':'same exact-name companion SRV as ordinary SpecRGB bridge'},
 'split_resource_contract':{'t1':'stock DSR SpecTex alpha/roughness','t10':'stock DSR g_Subsurf preserved','t13':'PTDE SpecRGB companion from DSRRL_PTDE_SPEC','normal_diffuse':'stock DSR retained; no Normal/Diffuse bridge in this build'},
 'receiver_identity':{'stock_exact_hashes':SUB_STOCK_HASHES,'metadata_records_added':[48,49,50],'families':[9,10,11],'stable_hemenv_only':True,'HemEnvLerp':False,'PointLight':False,'exact_selector_gate':True},
 'receiver_payloads':rb,
 'donor':{'registry_index':26,'previous_hash':OLD_SLOT26_HASH,'previous_hash_absent_from_current_581_mtd_binder':True,'clone_source_index':232,'clone_source_hash':PTDE_BODY_DONOR_HASH,'new_hash':DSBT_HASH,'route_flag':3},
 'draw_gate':{'route_index26_only':True,'exact_metadata_offsets':[hex(x) for x in meta_offsets],'companion_gate':'reuses certified old shared-sidecar gate using no-draw t10 prepare+restore preflight','t13_state_capacity':T13_STATE_COUNT,'capacity_failure':'fail-open before custom receiver selection'},
 't13_transaction':{'pre':'route flag3 only: certified t10 prepare -> PSGet t10 companion -> immediate t10 restore -> save prior t13 -> bind companion t13','post':'restore prior t13 and release PSGet ref','ordinary_routes':'tailcall existing t10 wrappers unchanged','stock_t10_during_draw':'preserved','PSGetShaderResources_vtable_offset':'0x248','PSSetShaderResources_vtable_offset':'0x40'},
 'metadata':{'old_count':48,'new_count':51,'old_bytes_copied_byte_identical':True,'init_limit':'0xb28','draw_bound_max_index':50},
 'helpers':{'create_rva':hex(CREATE_RVA),'selector_rva':hex(SELECT_RVA),'cleanup_rva':hex(CLEAN_RVA),'gate_rva':hex(GATE_RVA),'pre_rva':hex(PRE_RVA),'post_rva':hex(POST_RVA),'sizes':{'create':len(create),'selector':len(selector),'cleanup':len(cleanup),'gate':len(gate),'pre':len(pre),'post':len(post)},'subsurf_obj_bss':hex(SUB_OBJ_RVA),'t13_state_bss':[hex(T13_STATE_RVA),hex(T13_STATE_RVA+T13_STATE_COUNT*T13_STATE_STRIDE)]},
 'invariants':{'old_v13p_producer_transport_byte_identical':True,'old_srgblt_byte_identical':True,'old_srgbml_byte_identical':True,'stock_t10_subsurf_resource_preserved_by_payload':'3/3 PASS','new_t13_resource_sample':'3/3 PASS','b12_spec_material_island':'3/3 PASS','t14_envspec_branch':'3/3 PASS','stock_post_t13_pow22_removed':'3/3 PASS','non_SHEX_chunks_byte_identical_to_spec_only_subsurf':'3/3 PASS','new_payload_dxbc_checksums':'3/3 PASS','ptde_body_c101_1_0_record_preserved':True,'P[D]_no_spec_policy_unchanged':True,'plain_route_flags18_25_unchanged':True,'plain_spec_only_receivers_preserved':True,'DSR_SSS_preserved':True},
 'binary_diff_original_prefix':{'changed_bytes':len(changed),'section_counts':sc,'appended_bytes':len(b)-len(src)},
 'status_boundaries':{'construction':'PASS','static_compatibility':'PASS','runtime_liveness':'OPEN','subsurf_spec_only_activation':'OPEN','PTDE_c101_subsurf':'CONSTRUCTION_CONFIRMED','DSR_SSS_preservation':'CONSTRUCTION_CONFIRMED','pixel_behavior':'OPEN'}
}
AUD.write_text(json.dumps(audit,indent=2),encoding='utf-8')
README.write_text('''DSRRL Material Response 1.3 — Equipment Subsurf SPEC_ONLY RC

Purpose
- Complete the exact Ps_Body[DSBT] SpecRGB/material-response bridge while preserving DSR Subsurface Scattering as an independent Remaster operator.

Operator isolation
- t1: stock DSR SpecTex alpha/roughness
- t10: stock DSR g_Subsurf, preserved
- t13: exact-name PTDE SpecRGB companion
- b12[0]: exact PTDE Ps_Body[DSB] material/spec response, c101=1.0
- t14: retained Material Response EnvSpec A/B branch
- diffuse: stock DSR
- normals: stock DSR

The PTDE t13 sidecar is consumed in the same domain as the confirmed plain equipment bridge: the stock DSR LOG/*2.2/EXP block after the SpecRGB replacement is removed.

Exact receiver scope
- FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf.fpo
- FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf.fpo
- FRPG_Phn_DifSpcBmp__________HemEnvSubsurf.fpo
Stable HemEnv only. HemEnvLerp, PointLight and unrelated Subsurf families fail open.

Construction/static compatibility PASS. Runtime activation and PTDE-visible pixel behavior remain OPEN.
''',encoding='utf-8')
with zipfile.ZipFile(PKG,'w',zipfile.ZIP_DEFLATED) as z:
 for p in [OUT,AUD,README,BUILDER,TRANSFORMER,PREAUD]: z.write(p,p.name)
assert zipfile.ZipFile(PKG).testzip() is None
print(json.dumps({'candidate':str(OUT),'sha256':outsha,'size':OUT.stat().st_size,'package':str(PKG),'package_sha256':hashlib.sha256(PKG.read_bytes()).hexdigest(),'audit':str(AUD),'helper_sizes':audit['helpers']['sizes'],'metadata_rva':hex(META_RVA),'payload_rvas':[hex(x) for x in sub_rvas],'new_section_vs':hex(new_vs),'new_section_rs':hex(new_rs),'size_of_image':hex(struct.unpack_from('<I',b,opt+56)[0]),'changed_prefix_bytes':len(changed),'section_counts':sc},indent=2))
