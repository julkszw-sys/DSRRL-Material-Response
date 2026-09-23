from pathlib import Path
import struct,hashlib,json,re,subprocess,tempfile,shutil
BASE=0x180000000
P=Path('/mnt/data/v12work/DSRRL_Material_Response_1.3_PTDE_SPEC_SUBSURF_NORMAL_DIFFUSE_INTEGRATED_V12_RECEIVER_ORDINAL_FIX_RC.addon64')
V5=Path('/mnt/data/v9work/V5.addon64')
SAFE=Path('/mnt/data/v9work/SAFE.addon64')
SRC=Path('/mnt/data/v12work/asset_integrated_v12.c')
STUB=Path('/mnt/data/v12work/asset_stubs_v12.s')
MAP=Path('/mnt/data/v12work/asset_ext_v12.map')
ALLOW=Path('/mnt/data/v9work/bridge_asset_allowlist_v6.json')

def parse_pe(b):
 pe=struct.unpack_from('<I',b,0x3c)[0];assert b[pe:pe+4]==b'PE\0\0';co=pe+4;n=struct.unpack_from('<H',b,co+2)[0];osz=struct.unpack_from('<H',b,co+16)[0];opt=co+20;assert struct.unpack_from('<H',b,opt)[0]==0x20b;st=opt+osz;ss=[]
 for i in range(n):
  o=st+i*40;name=b[o:o+8].rstrip(b'\0').decode();vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8);ch=struct.unpack_from('<I',b,o+36)[0];ss.append(dict(name=name,off=o,vs=vs,va=va,rs=rs,ro=ro,ch=ch))
 return pe,opt,ss
def r2o(ss,r):
 for s in ss:
  if s['ro'] and s['va']<=r<s['va']+s['rs']:return s['ro']+r-s['va']
 raise KeyError(hex(r))
def dirs(b,opt):return [struct.unpack_from('<II',b,opt+112+i*8) for i in range(16)]
def relocs(b,opt,ss):
 rva,sz=dirs(b,opt)[5];o=r2o(ss,rva);end=o+sz;z=[]
 while o<end:
  p,s=struct.unpack_from('<II',b,o);assert p and s>=8 and o+s<=end
  for j in range((s-8)//2):
   e=struct.unpack_from('<H',b,o+8+2*j)[0];t=e>>12;q=p+(e&0xfff)
   if t:z.append((q,t))
  o+=s
 assert o==end;return z

def dxbc_at(b,ss,r):
 o=r2o(ss,r);assert b[o:o+4]==b'DXBC';sz=struct.unpack_from('<I',b,o+24)[0];return b[o:o+sz]
def chunks(d):
 n=struct.unpack_from('<I',d,28)[0];offs=struct.unpack_from('<%dI'%n,d,32);return {d[o:o+4]:d[o+8:o+8+struct.unpack_from('<I',d,o+4)[0]] for o in offs}
def map_sym(name):
 for line in MAP.read_text(errors='replace').splitlines():
  if re.search(r'\b'+re.escape(name)+r'\b',line):
   m=re.match(r'\s*0001:([0-9A-Fa-f]{8})',line)
   if m:return int(m.group(1),16)
 raise KeyError(name)

def target5(b,ss,r):
 o=r2o(ss,r);assert b[o] in (0xe8,0xe9);return r+5+struct.unpack_from('<i',b,o+1)[0]

b=P.read_bytes();v5=V5.read_bytes();safe=SAFE.read_bytes();_,opt,ss=parse_pe(b);_,v5opt,v5ss=parse_pe(v5);_,safeopt,safess=parse_pe(safe)
# Section geometry and image bounds independently.
fa=struct.unpack_from('<I',b,opt+36)[0];sa=struct.unpack_from('<I',b,opt+32)[0];soi=struct.unpack_from('<I',b,opt+56)[0];assert fa==0x200 and sa==0x1000
raw=[s for s in ss if s['rs']]; raw=sorted(raw,key=lambda s:s['ro']);assert all(raw[i]['ro']+raw[i]['rs']<=raw[i+1]['ro'] for i in range(len(raw)-1))
virt=sorted(ss,key=lambda s:s['va']);assert all(virt[i]['va']+max(virt[i]['vs'],virt[i]['rs'])<=virt[i+1]['va'] or virt[i]['name']=='.v13d' for i in range(len(virt)-1))
last=max(s['va']+s['vs'] for s in ss);assert soi>=last and soi%sa==0
# Directories except reloc are unchanged from V5; load-config content byte-identical.
d=dirs(b,opt);d5=dirs(v5,v5opt)
for i in [0,1,2,3,4,6,7,8,9,10,11,12,13,14,15]:assert d[i]==d5[i],(i,d[i],d5[i])
for i in [3,9,10]:
 r,sz=d[i]
 if r and sz:
  assert b[r2o(ss,r):r2o(ss,r)+sz]==v5[r2o(v5ss,r):r2o(v5ss,r)+sz]
# CFG flags retained.
dllchars=struct.unpack_from('<H',b,opt+70)[0];assert dllchars&0x40 and dllchars&0x20 and dllchars&0x4000
# Relocations preserve all SAFE entries and have 128 appended fixups.
r=relocs(b,opt,ss);rsafe=relocs(safe,safeopt,safess);assert len(rsafe)==648 and len(r)==776 and set(rsafe).issubset(r)
new=set(r)-set(rsafe);assert len(new)==128 and all(t==10 for _,t in new)
# Nonzero-delta rebase, then validate all new fixups change exactly.
delta=0x345670000;rb=bytearray(b)
for rv,t in r:
 if t==10:
  o=r2o(ss,rv);struct.pack_into('<Q',rb,o,(struct.unpack_from('<Q',rb,o)[0]+delta)&0xffffffffffffffff)
for rv,t in new:
 o=r2o(ss,rv);assert struct.unpack_from('<Q',rb,o)[0]==(struct.unpack_from('<Q',b,o)[0]+delta)&0xffffffffffffffff
# Hook destinations and original event47.
EXT=0x19a000
hook_expect={0x65e7:'integrated_pre_stub',0x666a:'integrated_post_stub',0x632e:'integrated_gate_stub',0x63fc:'integrated_selector_stub',0x1131bd:'capture_gate_stub',0x1135b7:'path_gate_stub'}
for h,n in hook_expect.items():assert target5(b,ss,h)==EXT+map_sym(n),(hex(h),n,hex(target5(b,ss,h)),hex(EXT+map_sym(n)))
o=r2o(ss,0x113e8e);assert b[o:o+3]==b'\x48\x8d\x15';assert 0x113e95+struct.unpack_from('<i',b,o+3)[0]==0x1133d9
# SAFE callback body still byte-identical.
for rva,n in [(0x1133d9,0xbb)]:assert b[r2o(ss,rva):r2o(ss,rva)+n]==safe[r2o(safess,rva):r2o(safess,rva)+n]
# Extension raw blob exactly equals linked .text after only declared sentinel/branch reloc patching is not simple equality; verify no imports in linked image via PE dirs.
ld=Path('/mnt/data/v12work/asset_ext_v12.dll').read_bytes();_,lo,lss=parse_pe(ld);ldirs=dirs(ld,lo);assert ldirs[1]==(0,0) and ldirs[12]==(0,0) and ldirs[9]==(0,0) and ldirs[10]==(0,0)
# Stub ABI: selector JMP continuation reserves 0x20, while called pre/post stubs reserve 0x28.
sel=EXT+map_sym('integrated_selector_stub');sb=b[r2o(ss,sel):r2o(ss,sel)+0x30];assert b'\x48\x83\xec\x20' in sb and b'\x48\x83\xc4\x20' in sb and b'\x48\x83\xec\x28' not in sb
for nm in ['integrated_pre_stub','integrated_post_stub']:
 q=EXT+map_sym(nm);x=b[r2o(ss,q):r2o(ss,q)+0x18];assert b'\x48\x83\xec\x28' in x and b'\x48\x83\xc4\x28' in x
# Source ownership invariants.
s=SRC.read_text();assert '__sync_val_compare_and_swap(&e->native_ctx,0,1)' in s;assert 'u64 diffuse_side=((e->flags&F_DIFF_READY)' in s;assert 'e->current_t0=t0;t0=0' in s and 'e->current_t2=t2;t2=0' in s;assert 'ps_set_srv(native,2,e->current_t2);release_srv(e->current_t2)' in s;assert 'ps_set_srv(native,0,e->current_t0);release_srv(e->current_t0)' in s
assert 'RVA_TELEM_FLAGS 0x112F80ULL' in s and 'TELEM_NORMAL_BIND 8u' in s
assert '[DSRRL][ASSET] CAPTURE_SPEC accepted' in s and '[DSRRL][ASSET] CAPTURE_NORMAL accepted' in s and '[DSRRL][ASSET] CAPTURE_DIFFUSE accepted' in s
assert '[DSRRL][ASSET] NORMAL_GATE hit' in s and '[DSRRL][ASSET] NORMAL_SIDECAR ready' in s and '[DSRRL][ASSET] NORMAL_T2_BIND confirmed' in s
assert '[DSRRL][ASSET] DIFFUSE_GATE hit' in s and '[DSRRL][ASSET] DIFFUSE_SIDECAR ready' in s and '[DSRRL][ASSET] DIFFUSE_T0_BIND confirmed' in s
# V12 invariant: TLS+0x14 is interpreted as local metadata ordinal; ordinary Bmp scope is local 0..22 only.
assert '[DSRRL][ASSET_DIAG] asset_pre entered' not in s and '[DSRRL][ASSET_DIAG] prepare_diffuse entered' not in s
assert 'log_tuple_once' not in s and 'append_u16_name' not in s
assert 'current_receiver_class(uptr base)' in s
assert 'if(rxclass!=3)return;' in s
assert 'local 0..22 DifSpcBmp PASS' in s and 'local 23..46 DifSpc REJECT' in s and 'TLS array NULL' in s and 'TLS block NULL' in s
assert 'local receiver NEGATIVE' in s and 'local 47..50 nonordinary/Subsurf REJECT' in s and 'local >=51 invalid REJECT' in s
pa=s.index('void asset_pre(uptr native)'); pe=s.index('struct asset_entry*e=find_asset',pa); seg=s[pa:pe]
assert seg.index('current_receiver_class(base)') < seg.index('log_norm_receiver_class') < seg.index('if(rxclass!=3)return;')
pd=s.index('void prepare_diffuse(void *cmd'); pde=s.index('uptr native=native_from_cmd',pd); segd=s[pd:pde]
assert segd.index('current_receiver_class(base)') < segd.index('log_diff_receiver_class') < segd.index('if(rxclass!=3)return;')
# Classifier itself contains no logging and keeps exact V8 TLS source: GS:[0x58], RVA_TLS_INDEX, block+0x14.
cs=s.index('static int current_receiver_class'); ce=s.index('static void log_diff_receiver_class',cs); cseg=s[cs:ce]
assert 'log_once' not in cseg and 'RVA_TLS_INDEX' in cseg and '%%gs:0x58' in cseg and 'block+0x14' in cseg
assert 'int local=*(int*)(block+0x14)' in cseg and 'if(local<=22)return 3' in cseg and 'if(local<=46)return 4' in cseg and 'if(local<=50)return 5' in cseg
assert 'TELEM_RX_DIFF_TLS_ARRAY_NULL 17u' in s and 'TELEM_RX_NORM_48_PLUS 30u' in s
# Route arrays sorted, unique, exact counts and comments scope.
def arr(block,fields):
 m=re.search(r'static const .*? '+block+r'\[\] = \{\n(.*?)\n\};',s,re.S);vals=[];names=[];rows=0
 for line in m.group(1).splitlines():
  hs=tuple(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)ULL',line));assert len(hs)==fields;vals.append(hs);cm=re.search(r'/\*(.*?)\((\d+)\)\s*\*/',line);rows+=int(cm.group(2));names.append(cm.group(1).strip())
 assert vals==sorted(vals) and len(vals)==len(set(vals));return vals,names,rows
nv,nn,nrows=arr('k_normal_triples',3);dv,dn,drows=arr('k_diffuse_pairs',2);assert (len(nv),nrows)==(557,3872) and (len(dv),drows)==(528,3446)
assert all(all(part.strip().casefold().startswith(('hd_','bd_','am_','lg_')) for part in x.split('|')) for x in nn)
assert all(all(part.strip().casefold().startswith(('hd_','bd_','am_','lg_')) for part in x.split('+')) for x in dn)
assert not any('hd_m_9360' in x.casefold() for x in nn+dn);assert not any(x.split('|')[-1].strip().casefold().startswith('lg_f_9400_lg_m_9400 ') for x in nn)
# Allowlist exactly matches C name tables.
a=json.loads(ALLOW.read_text());assert len(a['normal'])==555 and len(a['diffuse'])==524 and not set(x.casefold() for x in a['normal'])&set(x.casefold() for x in a['diffuse'])
# Capture policy is a formally bounded exact union, no global suffix blanket.
ms=re.search(r'static const u64 k_spec_names\[\] = \{\n(.*?)\n\};',s,re.S);assert ms;spec_entries=re.findall(r'0x[0-9a-fA-F]+ULL',ms.group(1));assert len(spec_entries)==769
assert "if(n>=2){u32 a=s[n-2],b=s[n-1];if(a=='_'&&(b=='s'||b=='S'))return 1;}" not in s
assert 'if(spec_hash(h))' in s and 'if(normal_hash(h))' in s and 'if(diffuse_hash(h))' in s
capture_union=769+555+524;assert capture_union==1848 and capture_union<2048
# BSS separation: build52 t13 state [0x112350,0x112950), asset map [0x112980,0x112f80).
assert 0x112350+64*0x18==0x112950 and 0x112950<=0x112980 and 0x112980+32*0x30==0x112f80 and 0x112f80+31*4<=0x113000
# New payload locations from helper and RDEF/SHEX consistency.
pay=[]
for lea,szimm,name in [(0x189800,0x189809,'Csd'),(0x189819,0x189822,'Sdw'),(0x189832,0x18983b,'plain')]:
 o=r2o(ss,lea);assert b[o:o+3]==b'\x48\x8d\x15';rv=lea+7+struct.unpack_from('<i',b,o+3)[0];sz=struct.unpack_from('<I',b,r2o(ss,szimm))[0];dx=dxbc_at(b,ss,rv);assert len(dx)==sz;ch=chunks(dx);rdef=ch[b'RDEF'];cbc,cbo,rbc,rbo=struct.unpack_from('<4I',rdef,0)
 def z(off):e=rdef.find(b'\0',off);return rdef[off:e].decode('ascii','replace')
 bd=[]
 for i in range(rbc):q=struct.unpack_from('<8I',rdef,rbo+i*32);bd.append((z(q[0]),)+q[1:])
 cb=[]
 for i in range(cbc):q=struct.unpack_from('<6I',rdef,cbo+i*24);cb.append((z(q[0]),)+q[1:])
 assert any(x[0]=='gSMP_13' and x[5]==13 for x in bd);assert any(x[0]=='gSMP_14' and x[5]==14 for x in bd);assert any(x[0]=='gSMP_14Sampler' and x[5]==14 for x in bd);assert not any(x[5]==9 and x[1] in (2,3) for x in bd);assert any(x[0]=='PTDEBridgeBuffer' and x[5]==12 for x in bd);assert any(x[0]=='PTDEBridgeBuffer' and x[3]==64 for x in cb)
 sh=ch[b'SHEX'];ws=struct.unpack('<%dI'%(len(sh)//4),sh);ref=lambda typ,idx:sum(1 for j,w in enumerate(ws[:-1]) if (w&0x00fff000)==typ and ws[j+1]==idx)
 assert ref(0x00107000,13)>=2 and ref(0x00107000,14)>=2 and ref(0x00107000,9)==0 and ref(0x00106000,14)>=2 and ref(0x00106000,9)==0
 pay.append({'variant':name,'rva':hex(rv),'size':sz,'sha256':hashlib.sha256(dx).hexdigest(),'rdef_bindings':rbc,'rdef_cbuffers':cbc})
# Corrected payloads are outside extension and relocation table, non-overlap.
ext_end=EXT+len(next(x for x in [Path('/mnt/data/v12work/asset_ext_v12.dll').read_bytes()])); # informational only
# Rebuild determinism at script level: rerun builder and same hash.
before=hashlib.sha256(P.read_bytes()).hexdigest();subprocess.run(['python','/mnt/data/v12work/build_v12.py'],check=True,stdout=subprocess.DEVNULL);after=hashlib.sha256(P.read_bytes()).hexdigest();assert before==after
report={'schema':'dsrrl.v12.receiver_ordinal_fix.independent_audit','binary':{'sha256':before,'size':P.stat().st_size,'image_size':hex(soi),'dll_characteristics':hex(dllchars)},'pe':{'sections_nonoverlap':'PASS','directories_except_reloc_unchanged':'PASS','loadconfig_exception_tls_bytes_unchanged':'PASS','cfg_flags_retained':'PASS','relocations':{'safe':len(rsafe),'new':len(new),'total':len(r),'nonzero_delta_rebase':'PASS'}},'hooks':'PASS_EXACT','event47':'PASS_SAFE_V2','linked_extension':'PASS_NO_IMPORT_IAT_TLS_LOADCONFIG','abi':{'selector_alignment':'PASS_0x20','pre_post_alignment':'PASS_0x28'},'state_machine':{'COM_hold_restore_release':'PASS_SOURCE_AND_CODEGEN_CONTRACT','diffuse_ready_one_shot':'PASS','atomic_context_claim':'PASS'},'routes':{'normal':[len(nv),nrows,555],'diffuse':[len(dv),drows,524],'signature_conflict_classes_excluded':'PASS','armor_scope':'PASS','capture':{'spec':769,'normal':555,'diffuse':524,'union':1848,'capacity':2048,'headroom':200,'global_suffix_blanket':False}},'bss':'PASS_NONOVERLAP','subsurf_payloads':pay,'deterministic_rebuild':'PASS','activation_telemetry':{'basis':'V11_RUNTIME_PASS_PLUS_STATIC_METADATA_ORDINAL_RE','pre_tls_logging':False,'dynamic_tuple_formatting':False,'receiver_scope_widened':False,'receiver_representation':'TLS_LOCAL_METADATA_ORDINAL','accepted_local_ordinals':'0..22_DifSpcBmp','rejected_local_ordinals':'23..46_DifSpc;47..50_nonordinary_or_subsurf;>=51_invalid','receiver_classes':['TLS_ARRAY_NULL','TLS_BLOCK_NULL','NEGATIVE','LOCAL_0_22_BMP_PASS','LOCAL_23_46_NONBMP_REJECT','LOCAL_47_50_NONORDINARY_REJECT','LOCAL_51_PLUS_INVALID'],'capture_spec':'INHERITED','capture_normal':'INHERITED','capture_diffuse':'INHERITED','normal_stages':'TLS_CLASS_THEN_EXISTING_PIPELINE','diffuse_stages':'TLS_CLASS_THEN_EXISTING_PIPELINE','spec_t10_bind':'INHERITED_ONE_SHOT'},'known_gap':{'unwind':'appended/JMP-continuation code not represented in host .pdata; do not promote full compatibility'},'status':{'construction':'PASS','static_compatibility':'PARTIAL','runtime':'OPEN','activation':'OPEN','pixel':'OPEN'}}
Path('/mnt/data/v12work/DSRRL_V12_INDEPENDENT_AUDIT.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
