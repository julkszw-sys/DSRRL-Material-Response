from pathlib import Path
import struct, hashlib, json, re, math
BASE=0x180000000
EXT_BASE=0x19A000
SRC=Path('/mnt/data/v9work/V5.addon64')
SAFE=Path('/mnt/data/v9work/SAFE.addon64')
LINKED=Path('/mnt/data/v12work/asset_ext_v12.dll')
MAP=Path('/mnt/data/v12work/asset_ext_v12.map')
PAYDIR=Path('/mnt/data/v9work/subsurf_rdef')
PAYFILES=[PAYDIR/'Csd_RDEF_FIXED.dxbc',PAYDIR/'Sdw_RDEF_FIXED.dxbc',PAYDIR/'plain_RDEF_FIXED.dxbc']
OUT=Path('/mnt/data/v12work/DSRRL_Material_Response_1.3_PTDE_SPEC_SUBSURF_NORMAL_DIFFUSE_INTEGRATED_V12_RECEIVER_ORDINAL_FIX_RC.addon64')
AUD=Path('/mnt/data/v12work/DSRRL_MR13_RENDERER_ASSET_BRIDGE_V12_STATIC_AUDIT.json')
OLD_META_RVA=0x7fd80; OLD_META_COUNT=48; META_STRIDE=0x38; NEW_META_RVA=0x189e00
SELECT_MOVABS_IMM=[0x189342,0x189355,0x189368]

def align(x,a):return (x+a-1)&~(a-1)
def parse_pe(b):
 pe=struct.unpack_from('<I',b,0x3c)[0];co=pe+4;n=struct.unpack_from('<H',b,co+2)[0];osz=struct.unpack_from('<H',b,co+16)[0];opt=co+20;st=opt+osz;secs=[]
 for i in range(n):
  o=st+i*40;name=b[o:o+8].rstrip(b'\0').decode('ascii');vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8);ch=struct.unpack_from('<I',b,o+36)[0];secs.append(dict(i=i,off=o,name=name,vs=vs,va=va,rs=rs,ro=ro,ch=ch))
 return pe,opt,secs
def r2o(secs,rva):
 for s in secs:
  if s['ro'] and s['va']<=rva<s['va']+s['rs']:return s['ro']+(rva-s['va'])
 raise ValueError(hex(rva))
def rel32(next_rva,target):return struct.pack('<i',target-next_rva)
def linked_text(path):
 b=path.read_bytes();_,_,ss=parse_pe(b);s=next(x for x in ss if x['name']=='.text');return bytearray(b[s['ro']:s['ro']+s['vs']])
def map_offsets(txt):
 names=['asset_push_wrapper','integrated_pre_stub','integrated_post_stub','capture_gate_stub','path_gate_stub','integrated_gate_stub','integrated_selector_stub','pre_chain_rel32','post_chain_rel32','capture_accept_rel32','capture_reject_rel32','path_asset_rel32','path_orig_rel32','gate_subsurf_rel32','selector_subsurf_rel32']
 out={}
 for line in txt.splitlines():
  for n in names:
   if re.search(r'\b'+re.escape(n)+r'\b',line):
    m=re.match(r'\s*0001:([0-9A-Fa-f]{8})',line)
    if m:out[n]=int(m.group(1),16)
 if set(out)!=set(names):raise RuntimeError(('map missing',set(names)-set(out)))
 return out
def parse_relocs(b,opt,secs):
 rr,rsz=struct.unpack_from('<II',b,opt+112+5*8);o=r2o(secs,rr);end=o+rsz;out=[]
 while o+8<=end:
  page,size=struct.unpack_from('<II',b,o)
  if not page or size<8 or o+size>end:break
  for j in range((size-8)//2):
   e=struct.unpack_from('<H',b,o+8+2*j)[0];typ=e>>12;off=e&0xfff
   if typ:out.append((page+off,typ))
  o+=size
 return rr,rsz,out
def build_reloc_blob(entries):
 pages={}
 for r,t in sorted(set(entries)):pages.setdefault(r&~0xfff,[]).append(((t&15)<<12)|(r&0xfff))
 z=bytearray()
 for p in sorted(pages):
  vals=sorted(set(pages[p]));
  if len(vals)&1:vals.append(0)
  z+=struct.pack('<II',p,8+2*len(vals))+struct.pack('<%dH'%len(vals),*vals)
 return bytes(z)

def dxbc_chunks(d):
 assert d[:4]==b'DXBC';n=struct.unpack_from('<I',d,28)[0];offs=struct.unpack_from('<%dI'%n,d,32);return [(d[o:o+4],d[o+8:o+8+struct.unpack_from('<I',d,o+4)[0]]) for o in offs]
def rdef_bindings(d):
 r=dict(dxbc_chunks(d))[b'RDEF'];cbc,cbo,rbc,rbo=struct.unpack_from('<4I',r,0)
 def z(off):e=r.find(b'\0',off);return r[off:e].decode('ascii','replace')
 bs=[]
 for i in range(rbc):q=struct.unpack_from('<8I',r,rbo+i*32);bs.append((z(q[0]),)+q[1:])
 cbs=[]
 for i in range(cbc):q=struct.unpack_from('<6I',r,cbo+i*24);cbs.append((z(q[0]),)+q[1:])
 return bs,cbs

src=bytearray(SRC.read_bytes()); safe=SAFE.read_bytes(); _,opt,secs=parse_pe(src); _,safeopt,safesecs=parse_pe(safe)
file_align=struct.unpack_from('<I',src,opt+36)[0];sect_align=struct.unpack_from('<I',src,opt+32)[0];sr=next(x for x in secs if x['name']=='.srgbmt')
# Prepare extension blob and patch self-base sentinel + branch placeholders.
blob=linked_text(LINKED);syms=map_offsets(MAP.read_text(errors='replace'))
neg=struct.pack('<Q',(-0x7A6B5C4D3E2F1908)&0xffffffffffffffff);idx=bytes(blob).find(neg);assert idx>=0 and bytes(blob).find(neg,idx+1)<0
push_rva=EXT_BASE+syms['asset_push_wrapper'];blob[idx:idx+8]=struct.pack('<Q',(-push_rva)&0xffffffffffffffff)
chains={'pre_chain_rel32':0x189900,'post_chain_rel32':0x189c00,'capture_accept_rel32':0x1131db,'capture_reject_rel32':0x1132fc,'path_asset_rel32':0x113668,'path_orig_rel32':0x1135bc,'gate_subsurf_rel32':0x189700,'selector_subsurf_rel32':0x189300}
for n,t in chains.items():off=syms[n];blob[off:off+4]=rel32(EXT_BASE+off+4,t)
# New corrected Subsurf payload placement after extension, page-aligned.
payloads=[p.read_bytes() for p in PAYFILES];pay_rvas=[];cursor=align(EXT_BASE+len(blob),0x1000)
for d in payloads:pay_rvas.append(cursor);cursor=align(cursor+len(d),16)
# Extend/zero .srgbmt enough for extension + moved payloads; relocation blob appended later.
content_end=cursor;new_vs=content_end-sr['va'];new_rs=align(new_vs,file_align);new_file_end=sr['ro']+new_rs
if len(src)<new_file_end:src.extend(b'\0'*(new_file_end-len(src)))
ext_off=sr['ro']+(EXT_BASE-sr['va']);src[ext_off:sr['ro']+new_rs]=b'\0'*(sr['ro']+new_rs-ext_off);src[ext_off:ext_off+len(blob)]=blob
for rva,d in zip(pay_rvas,payloads):o=sr['ro']+(rva-sr['va']);src[o:o+len(d)]=d
struct.pack_into('<I',src,sr['off']+8,new_vs);struct.pack_into('<I',src,sr['off']+16,new_rs);struct.pack_into('<I',src,opt+56,align(sr['va']+new_vs,sect_align))
_,opt,secs=parse_pe(src);sr=next(x for x in secs if x['name']=='.srgbmt')
# Hook helpers.
def patch_call(rva,target):o=r2o(secs,rva);assert src[o]==0xE8;src[o+1:o+5]=rel32(rva+5,target)
def patch_jmp5(rva,target):o=r2o(secs,rva);src[o:o+5]=b'\xE9'+rel32(rva+5,target)
patch_call(0x65e7,EXT_BASE+syms['integrated_pre_stub']);patch_call(0x666a,EXT_BASE+syms['integrated_post_stub']);patch_call(0x632e,EXT_BASE+syms['integrated_gate_stub']);patch_jmp5(0x63fc,EXT_BASE+syms['integrated_selector_stub'])
# Certified SAFE V2 event47 callback.
o=r2o(secs,0x113e8e);src[o:o+7]=b'\x48\x8d\x15'+rel32(0x113e95,0x1133d9)
# capture before suffix assumption + asset path builder.
cap=0x1131bd;capend=0x1131db;o=r2o(secs,cap);src[o:o+(capend-cap)]=b'\xE9'+rel32(cap+5,EXT_BASE+syms['capture_gate_stub'])+b'\x90'*((capend-cap)-5);patch_jmp5(0x1135b7,EXT_BASE+syms['path_gate_stub'])
# Patch both dormant eager helper and active lazy helper to corrected payloads/sizes.
# eager helper Subsurf slots: lea starts 0x1890ea/123/15c, size imm at +9? instruction sequence LEA7 then 41 b8 imm32 (imm at start+9).
for lea,size_imm,target,d in zip([0x1890ea,0x189123,0x18915c],[0x1890f3,0x18912c,0x189165],pay_rvas,payloads):
 o=r2o(secs,lea);assert src[o:o+3]==b'\x48\x8d\x15';src[o+3:o+7]=rel32(lea+7,target);struct.pack_into('<I',src,r2o(secs,size_imm),len(d))
# active lazy helper
for lea,size_imm,target,d in zip([0x189800,0x189819,0x189832],[0x189809,0x189822,0x18983b],pay_rvas,payloads):
 o=r2o(secs,lea);assert src[o:o+3]==b'\x48\x8d\x15';src[o+3:o+7]=rel32(lea+7,target);struct.pack_into('<I',src,r2o(secs,size_imm),len(d))
# Rebuild relocation surface from original SAFE base relocs + known moved absolute VAs.
_,_,safe_rels=parse_relocs(safe,safeopt,safesecs);assert len(safe_rels)==648
old_end=OLD_META_RVA+OLD_META_COUNT*META_STRIDE;old_meta=[(r,t) for r,t in safe_rels if OLD_META_RVA<=r<old_end];assert len(old_meta)==119 and all(t==10 for r,t in old_meta)
translated=[(NEW_META_RVA+(r-OLD_META_RVA),t) for r,t in old_meta]
new_sub=[]
for i in range(48,51):rr=NEW_META_RVA+i*META_STRIDE;new_sub += [(rr,10),(rr+0x28,10)]
new_req=translated+new_sub+[(r,10) for r in SELECT_MOVABS_IMM];assert len(new_req)==128 and len(set(new_req))==128
# Linked v7 extension must contain no absolute preferred-image pointer materialization after sentinel patch.
for off in range(0,len(blob)-9):
 if blob[off] in (0x48,0x49) and 0xB8<=blob[off+1]<=0xBF:
  v=struct.unpack_from('<Q',blob,off+2)[0];assert not (BASE<=v<BASE+0x400000),(hex(off),hex(v))
# New relocation table after all content.
merged=safe_rels+new_req;relblob=build_reloc_blob(merged);rel_rva=align(content_end,0x10);final_end=rel_rva+len(relblob);final_vs=final_end-sr['va'];final_rs=align(final_vs,file_align);final_file_end=sr['ro']+final_rs
if len(src)<final_file_end:src.extend(b'\0'*(final_file_end-len(src)))
ro=sr['ro']+(rel_rva-sr['va']);src[ro:ro+len(relblob)]=relblob
struct.pack_into('<I',src,sr['off']+8,final_vs);struct.pack_into('<I',src,sr['off']+16,final_rs);struct.pack_into('<I',src,opt+56,align(sr['va']+final_vs,sect_align));struct.pack_into('<II',src,opt+112+5*8,rel_rva,len(relblob))
OUT.write_bytes(src)
# Readback invariants.
b=OUT.read_bytes();_,opt2,ss2=parse_pe(b);rr,rsz,rels=parse_relocs(b,opt2,ss2);assert rr==rel_rva and set(safe_rels).issubset(set(rels)) and set(new_req).issubset(set(rels)) and len(rels)==776
# simulated rebase all DIR64.
delta=0x234560000;reb=bytearray(b)
for r,t in rels:
 if t==10:
  o=r2o(ss2,r);v=struct.unpack_from('<Q',reb,o)[0];struct.pack_into('<Q',reb,o,(v+delta)&0xffffffffffffffff)
for r,t in new_req:
 o=r2o(ss2,r);assert struct.unpack_from('<Q',reb,o)[0]==(struct.unpack_from('<Q',b,o)[0]+delta)&0xffffffffffffffff
# Hook target readback.
def ct(r):o=r2o(ss2,r);return r+5+struct.unpack_from('<i',b,o+1)[0]
assert ct(0x65e7)==EXT_BASE+syms['integrated_pre_stub'];assert ct(0x666a)==EXT_BASE+syms['integrated_post_stub'];assert ct(0x632e)==EXT_BASE+syms['integrated_gate_stub'];assert ct(0x63fc)==EXT_BASE+syms['integrated_selector_stub']
# event callback original.
o=r2o(ss2,0x113e8e);evt=0x113e95+struct.unpack_from('<i',b,o+3)[0];assert evt==0x1133d9
# corrected payload readback and RDEF.
paya=[]
for rva,d,name in zip(pay_rvas,payloads,['Csd','Sdw','plain']):
 o=r2o(ss2,rva);got=b[o:o+len(d)];assert got==d;binds,cbs=rdef_bindings(got);assert any(x[0]=='gSMP_13' and x[5]==13 for x in binds);assert any(x[0]=='gSMP_14' and x[5]==14 for x in binds);assert any(x[0]=='gSMP_14Sampler' and x[5]==14 for x in binds);assert any(x[0]=='PTDEBridgeBuffer' and x[5]==12 for x in binds);assert any(x[0]=='PTDEBridgeBuffer' and x[3]==64 for x in cbs);paya.append({'variant':name,'rva':hex(rva),'size':len(d),'sha256':hashlib.sha256(d).hexdigest()})
# lazy helper decodes to new targets/sizes.
for lea,size_imm,target,d in zip([0x189800,0x189819,0x189832],[0x189809,0x189822,0x18983b],pay_rvas,payloads):
 o=r2o(ss2,lea);got=lea+7+struct.unpack_from('<i',b,o+3)[0];assert got==target;assert struct.unpack_from('<I',b,r2o(ss2,size_imm))[0]==len(d)
# selector stack allocation is exactly 0x20 around diffuse_ready call in linked runtime blob.
sel=EXT_BASE+syms['integrated_selector_stub'];o=r2o(ss2,sel);sb=b[o:o+0x30];assert b'\x48\x83\xec\x20' in sb and b'\x48\x83\xc4\x20' in sb
# PE directories not intentionally changed except relocation; import/TLS/loadconfig/exception same as V5 source.
_,srcopt,srcsecs=parse_pe(SRC.read_bytes());dirs={}
for i,nm in [(1,'import'),(3,'exception'),(9,'tls'),(10,'load_config'),(12,'iat')]:dirs[nm]=(struct.unpack_from('<II',SRC.read_bytes(),srcopt+112+i*8),struct.unpack_from('<II',b,opt2+112+i*8));assert dirs[nm][0]==dirs[nm][1]
sha=hashlib.sha256(b).hexdigest()
aud={
 'schema':'dsrrl.mr13.single_addon.v12.receiver_ordinal_fix.static',
 'input_v5':{'sha256':hashlib.sha256(SRC.read_bytes()).hexdigest(),'size':SRC.stat().st_size},
 'output':{'file':OUT.name,'sha256':sha,'size':OUT.stat().st_size},
 'asset_bridge':{'normal':{'safe_triples':557,'safe_rows':3872,'unique_names':555},'diffuse':{'safe_pairs':528,'safe_rows':3446,'unique_names':524},'normal_diffuse_name_intersection':0,'excluded_signature_conflict':'HD_M_9360 class','excluded_suffixless_normal':'LG_F_9400_LG_M_9400','scope':'HD/BD/AM/LG only'},
 'code_hardening':{'com_lifetime':'PSGet refs retained for replaced t0/t2 until post restore then Release','stale_transaction_cleanup':True,'selector_stack_alignment':'sub/add rsp 0x20 at JMP-continuation','event47':'SAFE V2 original 0x1133d9','extension_imports':'none by linked-image audit','dormant_push_wrapper':'no asset-state side effects'},
 'subsurf_dxbc':{'payloads':paya,'SHEX':'unchanged from build52 SPEC_ONLY payloads','RDEF':'corrected: t13 + t14/s14 + PTDEBridgeBuffer b12[4], no t9/s9','checksums':'PASS','non_RDEF_chunks_except_container_layout':'byte-identical to prior payload chunks'},
 'pe':{'relocations':{'legacy':648,'new_dir64':128,'merged':776,'directory_rva':hex(rel_rva),'rebase_delta':hex(delta),'simulation':'PASS'},'unchanged_directories':dirs,'cfg_load_config':'unchanged from V5/SAFE lineage','unwind':'GAP: new JMP-continuation/C extension code is outside host .pdata; no false compatibility promotion'},
 'activation_telemetry':{'basis':'V11 runtime-PASS + static TLS metadata-ordinal RE','new_logging_position':'after TLS array/block/local-ordinal read; before route return','dynamic_name_formatting':False,'receiver_scope_widened':False,'receiver_representation_fix':'TLS local metadata ordinal, not canonical receiver index','accepted_local_ordinals':'0..22 DifSpcBmp only','receiver_classes':['TLS array NULL','TLS block NULL','NEGATIVE','local 0..22 DifSpcBmp PASS','local 23..46 DifSpc REJECT','local 47..50 nonordinary/Subsurf REJECT','local >=51 invalid REJECT'],'spec_capture':'inherited','normal_capture':'inherited','diffuse_capture':'inherited','normal_stages':['receiver class','exact lookup pass','safe tuple reject','existing gate/ready/bind'],'diffuse_stages':['receiver class','exact lookup pass','safe pair reject','existing gate/ready/bind'],'legacy_spec_bind':'inherited T10_PTDE_BIND confirmed'},
 'status':{'construction':'PASS','static_compatibility':'PARTIAL_UNWIND_GAP','runtime_liveness':'OPEN','bridge_activation':'OPEN','pixel_behavior':'OPEN'}
}
AUD.write_text(json.dumps(aud,indent=2),encoding='utf-8')
print(json.dumps({'out':str(OUT),'sha256':sha,'size':OUT.stat().st_size,'extension_size':len(blob),'payload_rvas':[hex(x) for x in pay_rvas],'reloc_rva':hex(rel_rva),'size_of_image':hex(struct.unpack_from('<I',b,opt2+56)[0]),'audit':str(AUD)},indent=2))
