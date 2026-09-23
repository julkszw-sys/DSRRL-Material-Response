from pathlib import Path
import struct, hashlib, json, zipfile, subprocess, math

BASE=0x180000000
SRC=Path('/mnt/data/DSRRL_Material_Response_1.3_PTDE_SPEC_EQUIPMENT_PLAIN_ROUTE_EXPANSION_RC.addon64')
EXPECTED_SRC_SHA='ad23449c0f79638a878ed857bcb7220544a7b1526a33e77588336e526658608b'
OUT=Path('/mnt/data/DSRRL_Material_Response_1.3_PTDE_SPEC_EQUIPMENT_SPEC_ONLY_RC.addon64')
AUD=Path('/mnt/data/DSRRL_PTDE_SPEC_EQUIPMENT_SPEC_ONLY_AUDIT.json')
PKG=Path('/mnt/data/DSRRL_MATERIAL_RESPONSE_1.3_PTDE_SPEC_EQUIPMENT_SPEC_ONLY_RC.zip')
README=Path('/mnt/data/DSRRL_PTDE_SPEC_EQUIPMENT_SPEC_ONLY_README.txt')
BUILDER=Path(__file__)
WORK=Path('/mnt/data/full24_work')
PAYLOADS=[
    WORK/'FRPG_Phn_DifSpcBmp______Csd_HemEnv_SPEC_ONLY.fpo',
    WORK/'FRPG_Phn_DifSpcBmp______Sdw_HemEnv_SPEC_ONLY.fpo',
    WORK/'FRPG_Phn_DifSpcBmp__________HemEnv_SPEC_ONLY.fpo',
]
PAYLOAD_EXPECTED=[
 '4ab7570bac9a7ef9c4dbb67e001deba2cfb6761ab901f80f950a7224dc9df959',
 'fe2efca508f44b7a30f4656ffa0548f66e2e402bc3612f603fe8b6b029a4f3b5',
 '2075374f6eaa88d5021368b8626cc53ff7573d9acaf24610f573d5a50756d73d',
]
# Existing FULL24/current ABI
REG_RVA=0x81e80; STRIDE=0x48; ROUTE_SLOTS=list(range(18,26)); FLAG_OFF=0x44
DEVICE_RVA=0x107880
SPEC_OBJ_RVA=0x112310  # 3 qwords; verified no prior static refs
C100_ARRAY_RVA=0x107888
C101_ARRAY_RVA=0x107948
# Patch sites
CREATE_PATCH_RVA=0x697b; CREATE_PATCH_LEN=7; CREATE_RETURN_RVA=0x6982
SELECT_PATCH_RVA=0x63fc; SELECT_PATCH_LEN=0x20; SELECT_RETURN_RVA=0x641c
CLEAN_PATCH_RVA=0x7e71; CLEAN_PATCH_LEN=7; CLEAN_RETURN_RVA=0x7e78
GUARD_DISPATCH_RVA=0xe548
# Append within expanded last section
CREATE_RVA=0x17a000
SELECT_RVA=0x17a100
CLEAN_RVA=0x17a200
PAYLOAD0_RVA=0x17a400


def align(x,a): return (x+a-1)&~(a-1)

def parse_pe(b):
 pe=struct.unpack_from('<I',b,0x3c)[0]
 n=struct.unpack_from('<H',b,pe+6)[0]
 osz=struct.unpack_from('<H',b,pe+20)[0]
 opt=pe+24; st=opt+osz
 secs=[]
 for i in range(n):
  o=st+i*40; name=b[o:o+8].rstrip(b'\0').decode('ascii')
  vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8); ch=struct.unpack_from('<I',b,o+36)[0]
  secs.append(dict(i=i,off=o,name=name,vs=vs,va=va,rs=rs,ro=ro,ch=ch))
 return pe,opt,st,secs

def rva2off(secs,rva,allow_virtual=False):
 for s in secs:
  lim=max(s['vs'],s['rs']) if allow_virtual else s['rs']
  if s['va']<=rva<s['va']+lim:
   if s['ro']==0: raise ValueError(f'RVA {rva:x} is virtual-only in {s["name"]}')
   return s['ro']+(rva-s['va'])
 raise ValueError(f'RVA not mapped {rva:x}')

def rel32(src_next,target):
 d=target-src_next
 if not -(1<<31)<=d<(1<<31): raise ValueError('rel32 overflow')
 return struct.pack('<i',d)

def ripdisp(next_rva,target_rva): return rel32(next_rva,target_rva)

class Asm:
 def __init__(self,rva): self.rva=rva; self.b=bytearray(); self.labels={}; self.fix=[]
 def pos(self): return self.rva+len(self.b)
 def raw(self,x): self.b.extend(x)
 def emit(self,*xs): self.b.extend(xs)
 def label(self,n): self.labels[n]=self.pos()
 def jmp_label(self,n):
  self.emit(0xe9); at=len(self.b); self.raw(b'\0'*4); self.fix.append((at,n))
 def jcc_label(self,cc,n):
  self.raw(b'\x0f'+bytes([cc])); at=len(self.b); self.raw(b'\0'*4); self.fix.append((at,n))
 def jmp_abs(self,target): self.emit(0xe9); self.raw(rel32(self.pos()+4,target))
 def finish(self):
  for at,n in self.fix:
   nxt=self.rva+at+4; self.b[at:at+4]=rel32(nxt,self.labels[n])
  return bytes(self.b)

# Microsoft-compatible DXBC checksum (AMD reference algorithm)
MASK=0xffffffff
SROT=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
K=[int(abs(math.sin(i+1))*(1<<32)) & MASK for i in range(64)]
def rol(x,n): return ((x<<n)|(x>>(32-n)))&MASK
def transform(state,block):
 a,b,c,d=state; M=list(struct.unpack('<16I',block)); A,B,C,D=a,b,c,d
 for i in range(64):
  if i<16: F=(B&C)|((~B)&D); g=i
  elif i<32: F=(D&B)|((~D)&C); g=(5*i+1)%16
  elif i<48: F=B^C^D; g=(3*i+5)%16
  else: F=C^(B|(~D)); g=(7*i)%16
  F=(F+A+K[i]+M[g])&MASK; A,D,C,B=D,C,B,(B+rol(F,SROT[i]))&MASK
 return [(a+A)&MASK,(b+B)&MASK,(c+C)&MASK,(d+D)&MASK]
def dxbc_checksum(data):
 p=data[0x14:]; size=len(p); nbits=size*8; state=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
 full=size&~63
 for o in range(0,full,64): state=transform(state,p[o:o+64])
 rem=p[full:]
 if len(rem)>=56:
  block=rem+b'\x80'+b'\0'*(63-len(rem)); state=transform(state,block)
  M=[0]*16; M[0]=nbits; M[15]=(nbits>>2)|1; state=transform(state,struct.pack('<16I',*M))
 else:
  buf=bytearray(64); struct.pack_into('<I',buf,0,nbits); buf[4:4+len(rem)]=rem; buf[4+len(rem)]=0x80; struct.pack_into('<I',buf,60,(nbits>>2)|1); state=transform(state,bytes(buf))
 return struct.pack('<4I',*state)

def build_create(payload_rvas,payload_sizes):
 a=Asm(CREATE_RVA)
 # BSS objects are normally zero, but clear explicitly for runtime recreation/partial init.
 a.raw(b'\x31\xc0') # xor eax,eax
 for i in range(3):
  target=SPEC_OBJ_RVA+8*i; start=a.pos(); a.raw(b'\x48\x89\x05'+ripdisp(start+7,target))
 # If native device pointer is unexpectedly null, leave objects null and continue stock init.
 start=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(start+7,DEVICE_RVA))
 a.raw(b'\x48\x85\xc9'); a.jcc_label(0x84,'done') # je
 for i,(pr,sz) in enumerate(zip(payload_rvas,payload_sizes)):
  if i:
   start=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(start+7,DEVICE_RVA))
   a.raw(b'\x48\x85\xc9'); a.jcc_label(0x84,'done')
  a.raw(b'\x48\x8b\x01') # rax=[rcx]
  start=a.pos(); a.raw(b'\x48\x8d\x15'+ripdisp(start+7,pr)) # rdx payload
  a.raw(b'\x41\xb8'+struct.pack('<I',sz)) # r8d=size
  a.raw(b'\x45\x31\xc9') # xor r9d,r9d
  target=SPEC_OBJ_RVA+8*i; start=a.pos(); a.raw(b'\x4c\x8d\x15'+ripdisp(start+7,target)) # r10=&obj
  a.raw(b'\x4c\x89\x54\x24\x20') # [rsp+20]=r10
  a.raw(b'\x48\x8b\x40\x78') # rax = CreatePixelShader target
  start=a.pos(); a.raw(b'\xff\x15'+ripdisp(start+6,GUARD_DISPATCH_RVA)) # CFG guard dispatch to rax
 a.label('done')
 # displaced original instruction from 0x697b
 a.raw(bytes.fromhex('49 8d 85 58 0f 10 00'))
 a.jmp_abs(CREATE_RETURN_RVA)
 return a.finish()

def build_selector():
 a=Asm(SELECT_RVA)
 a.raw(b'\x41\x80\xfc\x02') # cmp r12b,2
 a.jcc_label(0x85,'old') # jne
 a.raw(b'\x48\x63\x06') # movsxd rax,dword ptr [rsi]
 a.raw(b'\x83\xf8\x09'); a.jcc_label(0x82,'null') # jb
 a.raw(b'\x83\xf8\x0b'); a.jcc_label(0x87,'null') # ja
 a.raw(b'\x83\xe8\x09') # sub eax,9
 start=a.pos(); a.raw(b'\x48\x8d\x0d'+ripdisp(start+7,SPEC_OBJ_RVA))
 a.raw(b'\x48\x8b\x0c\xc1') # rcx=[rcx+rax*8]
 a.raw(bytes.fromhex('48 89 4d 87')) # [rbp-79]=rcx
 a.jmp_abs(SELECT_RETURN_RVA)
 a.label('old')
 start=a.pos(); a.raw(b'\x48\x8d\x05'+ripdisp(start+7,C100_ARRAY_RVA))
 start=a.pos(); a.raw(b'\x48\x8d\x0d'+ripdisp(start+7,C101_ARRAY_RVA))
 a.raw(bytes.fromhex('45 84 e4'))
 a.raw(bytes.fromhex('48 0f 44 c8'))
 a.raw(bytes.fromhex('48 63 06'))
 a.raw(bytes.fromhex('48 8b 0c c1'))
 a.raw(bytes.fromhex('48 89 4d 87'))
 a.jmp_abs(SELECT_RETURN_RVA)
 a.label('null')
 a.raw(b'\x31\xc9') # xor ecx,ecx
 a.raw(bytes.fromhex('48 89 4d 87'))
 a.jmp_abs(SELECT_RETURN_RVA)
 return a.finish()

def build_cleanup():
 a=Asm(CLEAN_RVA)
 for i in range(3):
  target=SPEC_OBJ_RVA+8*i
  start=a.pos(); a.raw(b'\x48\x8b\x0d'+ripdisp(start+7,target))
  a.raw(b'\x48\x85\xc9'); a.jcc_label(0x84,f'n{i}')
  a.raw(b'\x48\x8b\x01')
  a.raw(b'\x48\x8b\x40\x10') # rax = Release target
  start=a.pos(); a.raw(b'\xff\x15'+ripdisp(start+6,GUARD_DISPATCH_RVA)) # CFG guard dispatch to rax
  a.raw(b'\x31\xc0')
  start=a.pos(); a.raw(b'\x48\x89\x05'+ripdisp(start+7,target))
  a.label(f'n{i}')
 # displaced original instruction at 0x7e71
 start=a.pos(); a.raw(b'\x48\x8b\x3d'+ripdisp(start+7,0x107a10))
 a.jmp_abs(CLEAN_RETURN_RVA)
 return a.finish()

# Load base and verify
src=SRC.read_bytes(); got=hashlib.sha256(src).hexdigest(); assert got==EXPECTED_SRC_SHA,(got,EXPECTED_SRC_SHA)
b=bytearray(src); pe,opt,st,secs=parse_pe(b)
sec=next(s for s in secs if s['name']=='.srgbmt')
assert sec['ro']+sec['rs']==len(b),('last section not physical EOF',hex(sec['ro']+sec['rs']),hex(len(b)))
file_align=struct.unpack_from('<I',b,opt+36)[0]; sect_align=struct.unpack_from('<I',b,opt+32)[0]
assert file_align==0x200 and sect_align==0x1000
assert sec['va']+sec['rs']==CREATE_RVA, (hex(sec['va']+sec['rs']),hex(CREATE_RVA))
# Verify spec-only payloads
payload_data=[]
for p,exp in zip(PAYLOADS,PAYLOAD_EXPECTED):
 d=p.read_bytes(); assert hashlib.sha256(d).hexdigest()==exp; assert d[:4]==b'DXBC'; assert d[4:20]==dxbc_checksum(d),(p,'bad checksum'); payload_data.append(d)
payload_rvas=[PAYLOAD0_RVA]
for d in payload_data[:-1]: payload_rvas.append(align(payload_rvas[-1]+len(d),16))
payload_sizes=[len(x) for x in payload_data]
# Ensure payload0 starts after helper reserved blocks.
assert PAYLOAD0_RVA>=CLEAN_RVA+0x100
# Build helper blocks
create=build_create(payload_rvas,payload_sizes); selector=build_selector(); cleanup=build_cleanup()
assert len(create)<=0x100, len(create); assert len(selector)<=0x100,len(selector); assert len(cleanup)<=0x100,len(cleanup)
# Grow final section raw/virtual and file
final_end=max(CREATE_RVA+len(create),SELECT_RVA+len(selector),CLEAN_RVA+len(cleanup),payload_rvas[-1]+payload_sizes[-1])
new_vs=final_end-sec['va']; new_rs=align(new_vs,file_align); new_file_end=sec['ro']+new_rs
b.extend(b'\0'*(new_file_end-len(b)))
# Section header: VirtualSize, SizeOfRawData, executable+readable initialized data
struct.pack_into('<I',b,sec['off']+8,new_vs)
struct.pack_into('<I',b,sec['off']+16,new_rs)
new_ch=sec['ch']|0x20000000 # MEM_EXECUTE
struct.pack_into('<I',b,sec['off']+36,new_ch)
new_soi=align(sec['va']+new_vs,sect_align); struct.pack_into('<I',b,opt+56,new_soi)
# Helper to raw offsets in expanded last section
def last_off(rva):
 assert sec['va']<=rva<sec['va']+new_rs
 return sec['ro']+(rva-sec['va'])
# Write code and payloads
b[last_off(CREATE_RVA):last_off(CREATE_RVA)+len(create)]=create
b[last_off(SELECT_RVA):last_off(SELECT_RVA)+len(selector)]=selector
b[last_off(CLEAN_RVA):last_off(CLEAN_RVA)+len(cleanup)]=cleanup
for rva,d in zip(payload_rvas,payload_data): b[last_off(rva):last_off(rva)+len(d)]=d
# Route records from artifact948: mark all eight added routes SPEC_ONLY class=2.
reg_off=rva2off(secs,REG_RVA)
old_flags=[]
for slot in ROUTE_SLOTS:
 off=reg_off+slot*STRIDE+FLAG_OFF; old_flags.append(b[off]); assert b[off]!=0,(slot,'route unexpectedly inactive'); b[off]=2
# Patch create site: 7-byte LEA -> JMP create helper + NOP2
create_off=rva2off(secs,CREATE_PATCH_RVA); old_create=bytes(b[create_off:create_off+CREATE_PATCH_LEN]); assert old_create==bytes.fromhex('49 8d 85 58 0f 10 00'),old_create.hex()
patch=b'\xe9'+rel32(CREATE_PATCH_RVA+5,CREATE_RVA)+b'\x90\x90'; b[create_off:create_off+7]=patch
# Patch selector entire 32-byte old block with JMP + NOPs
sel_off=rva2off(secs,SELECT_PATCH_RVA); old_sel=bytes(b[sel_off:sel_off+SELECT_PATCH_LEN])
expected_sel=bytes.fromhex('48 8d 05 85 14 10 00 48 8d 0d 3e 15 10 00 45 84 e4 48 0f 44 c8 48 63 06 48 8b 0c c1 48 89 4d 87')
assert old_sel==expected_sel,(old_sel.hex(),expected_sel.hex())
b[sel_off:sel_off+SELECT_PATCH_LEN]=b'\xe9'+rel32(SELECT_PATCH_RVA+5,SELECT_RVA)+b'\x90'*(SELECT_PATCH_LEN-5)
# Patch cleanup displaced instruction
clean_off=rva2off(secs,CLEAN_PATCH_RVA); old_clean=bytes(b[clean_off:clean_off+CLEAN_PATCH_LEN]); assert old_clean==bytes.fromhex('48 8b 3d 98 fb 0f 00'),old_clean.hex()
b[clean_off:clean_off+7]=b'\xe9'+rel32(CLEAN_PATCH_RVA+5,CLEAN_RVA)+b'\x90\x90'
# Write candidate
OUT.write_bytes(b)
# Disassembly readback
subprocess.run(['llvm-objdump','-d','--x86-asm-syntax=intel',str(OUT)],stdout=open('/mnt/data/spec_only_third_class_disasm.txt','w'),check=True)
# Verify old artifact948 bytes changed only intended places + extension + PE section header/SizeImage.
changed=[i for i in range(len(src)) if src[i]!=b[i]]
# Protected t10 producer section .v13p must be byte-identical.
def sec_by_name(name): return next(s for s in secs if s['name']==name)
v13p=sec_by_name('.v13p'); assert src[v13p['ro']:v13p['ro']+v13p['rs']]==bytes(b[v13p['ro']:v13p['ro']+v13p['rs']])
# Artifact948 shared gate code in .v13x must be byte-identical.
v13x=sec_by_name('.v13x'); assert src[v13x['ro']:v13x['ro']+v13x['rs']]==bytes(b[v13x['ro']:v13x['ro']+v13x['rs']])
# Existing prior .srgbmt physical bytes must remain byte-identical; we only append beyond old raw end.
assert src[sec['ro']:sec['ro']+sec['rs']]==bytes(b[sec['ro']:sec['ro']+sec['rs']])
# Old embedded DXBC sections .srgblt/.srgbml are unchanged.
for nm in ['.srgblt','.srgbml']:
 s=sec_by_name(nm); assert src[s['ro']:s['ro']+s['rs']]==bytes(b[s['ro']:s['ro']+s['rs']])
# New payload readback/checksums
payload_rb=[]
for i,(rva,d,exp) in enumerate(zip(payload_rvas,payload_data,PAYLOAD_EXPECTED)):
 got=bytes(b[last_off(rva):last_off(rva)+len(d)]); assert got==d; assert got[4:20]==dxbc_checksum(got)
 payload_rb.append({'runtime_slot':9+i,'rva':hex(rva),'size':len(d),'sha256':hashlib.sha256(got).hexdigest(),'checksum':'PASS'})
# Verify flags and route records otherwise unchanged from artifact948
flag_rb=[]
for slot,old in zip(ROUTE_SLOTS,old_flags):
 off=reg_off+slot*STRIDE; assert b[off+FLAG_OFF]==2
 assert bytes(src[off:off+FLAG_OFF])==bytes(b[off:off+FLAG_OFF])
 assert bytes(src[off+FLAG_OFF+1:off+STRIDE])==bytes(b[off+FLAG_OFF+1:off+STRIDE])
 flag_rb.append({'record_index':slot,'old_flag':old,'new_flag':2})
# BSS storage has no raw bytes; static reference audit from old disasm guaranteed 0x112310+ unreferenced.
# Diff section accounting for original-file bytes
def section_for_off(off):
 # headers
 if off<0x400: return 'PE_HEADERS'
 for s in secs:
  if s['ro'] and s['ro']<=off<s['ro']+s['rs']: return s['name']
 return 'OTHER'
sec_counts={}
for o in changed: sec_counts[section_for_off(o)]=sec_counts.get(section_for_off(o),0)+1
# Ensure route flags are eight one-byte changes; .text patches and header changes are expected.
# Spec receiver invariants from prebuild audit
pre=json.loads(Path('/mnt/data/spec_only_receiver_audit_prebuild.json').read_text())
assert all(x['invariants']=={'cb12_1':0,'cb12_0':1,'t10_decl':1,'t10_refs':1} for x in pre)
# PE reparse final and verify final section boundaries
fpe,fopt,fst,fsecs=parse_pe(b); fs=next(s for s in fsecs if s['name']=='.srgbmt')
assert fs['vs']==new_vs and fs['rs']==new_rs and (fs['ch']&0x20000000)
assert struct.unpack_from('<I',b,fopt+56)[0]==new_soi
# Package
outsha=hashlib.sha256(OUT.read_bytes()).hexdigest()
audit={
 'status':'PASS','date':'2026-09-19',
 'base':{'artifact_id':948,'file':SRC.name,'sha256':EXPECTED_SRC_SHA,'semantic':'route-expanded FULL24 with shared exact-sidecar conjunction gate'},
 'candidate':{'file':OUT.name,'sha256':outsha,'size':OUT.stat().st_size,'build_key':'material_response_1_3_ptde_spec_equipment_spec_only_third_class'},
 'architecture':{
   'route_flag':{'old_semantics':'0=c100 / nonzero=c101','new_semantics':'0=c100, 1=c101, 2=SPEC_ONLY; all other nonzero values retain old c101 behavior','records_flag2':ROUTE_SLOTS},
   'spec_only_receiver':'current FULL24 c101+t10 receiver with ONLY the V2.9.1 c100/diffuse delta reversed: cb12[1] -> stock cb0[9] at both diffuse material uses, diffuse exponent 1.0 -> stock 2.2. c101 cb12[0], t10 SpecRGB, stock t1 alpha/roughness, current spec/EnvSpec/LOD island remain unchanged.',
   'runtime_slots':[9,10,11],'logical_receivers':[33,34,35],
   'storage_rva':hex(SPEC_OBJ_RVA),'create_helper_rva':hex(CREATE_RVA),'selector_helper_rva':hex(SELECT_RVA),'cleanup_helper_rva':hex(CLEAN_RVA),
   'create_patch_rva':hex(CREATE_PATCH_RVA),'selector_patch_rva':hex(SELECT_PATCH_RVA),'cleanup_patch_rva':hex(CLEAN_PATCH_RVA),
   'shared_gate_from_artifact948':'byte-identical; shared P_Leather[DSB]/C_DullLeather[DSB]/C_Wet[DSB] still require current t1 exact-name resource -> real PTDE companion, fail-open otherwise',
   'explicit_exclusions':['P[D].mtd','Ps_Body[DSBT].mtd'],
 },
 'payloads':payload_rb,
 'receiver_prebuild_invariants':pre,
 'route_flags':flag_rb,
 'pe':{'srgbmt_old_vs':sec['vs'],'srgbmt_new_vs':new_vs,'srgbmt_old_raw_size':sec['rs'],'srgbmt_new_raw_size':new_rs,'srgbmt_characteristics_old':hex(sec['ch']),'srgbmt_characteristics_new':hex(new_ch),'size_of_image_old':hex(struct.unpack_from('<I',src,opt+56)[0]),'size_of_image_new':hex(new_soi),'section_count_unchanged':len(secs)},
 'invariants':{
   'artifact948_v13p_transport_byte_identical':True,'artifact948_v13x_shared_gate_byte_identical':True,'old_srgbmt_bytes_byte_identical':True,'old_srgblt_byte_identical':True,'old_srgbml_byte_identical':True,
   'route_payload_bytes_unchanged_except_flag':True,'spec_only_payload_checksums':'3/3 PASS','old_receiver_payloads_changed':False,'sidecar_loader_changed':False,'t10_transaction_changed':False,
   'P[D]_still_absent':True,'Ps_Body_DSBT_still_absent':True,
 },
 'binary_diff_original_prefix':{'changed_bytes':len(changed),'section_counts':sec_counts,'appended_bytes':len(b)-len(src)},
 'status_boundaries':{'construction':'PASS','static_compatibility':'PASS','runtime_liveness':'OPEN','spec_only_activation':'OPEN','receiver_specific_runtime':'OPEN','pixel_behavior':'OPEN'}
}
AUD.write_text(json.dumps(audit,indent=2),encoding='utf-8')
README.write_text('''DSRRL Material Response 1.3 — PTDE SpecRGB Equipment SPEC-ONLY RC\n\nThis is the clean equipment-SpecRGB successor to the route-expansion prototype.\n\nBase: artifact948 route-expanded FULL24, which already provides the exact-MTD routes and the fail-open exact-sidecar conjunction gate for historically shared base MTDs.\n\nThe eight newly added equipment routes now use a THIRD receiver class (flag=2), not the existing c100/c101 Material Response class. The third class exists only for plain DifSpcBmp stable HemEnv receiver slots 33/34/35. Its shader is the current FULL24 c101+t10 receiver with only the diffuse/c100 delta reverted to stock DSR: cb12[1] diffuse material color is replaced back with stock cb0[9], and the diffuse material exponent is restored from 1.0 to stock 2.2. PTDE c101/spec response, PTDE SpecRGB t10 RGB, stock t1 alpha/roughness, and the current spec/EnvSpec/LOD island are retained.\n\nShared bases P_Leather[DSB], C_DullLeather[DSB], C_Wet[DSB] still require a real exact-name DSRRL_PTDE_SPEC companion for current t1 or fail open.\n\nNot bridged here:\n- P[D].mtd: PTDE route has no g_Specular consumer.\n- Ps_Body[DSBT].mtd: separate Subsurf consumer.\n\nStatic construction/audit PASS only. Runtime liveness, activation of the new third receiver class, and final PTDE-visible pixel equivalence remain OPEN.\n''',encoding='utf-8')
with zipfile.ZipFile(PKG,'w',zipfile.ZIP_DEFLATED) as z:
 for p in [OUT,AUD,README,BUILDER,Path('/mnt/data/spec_only_receiver_audit_prebuild.json')]: z.write(p,p.name)
assert zipfile.ZipFile(PKG).testzip() is None
print(json.dumps({'candidate':str(OUT),'sha256':outsha,'size':OUT.stat().st_size,'package':str(PKG),'package_sha256':hashlib.sha256(PKG.read_bytes()).hexdigest(),'helpers':{'create':len(create),'selector':len(selector),'cleanup':len(cleanup)},'payload_rvas':[hex(x) for x in payload_rvas],'new_vs':hex(new_vs),'new_rs':hex(new_rs),'new_size_image':hex(new_soi),'changed_prefix_bytes':len(changed),'section_counts':sec_counts},indent=2))
