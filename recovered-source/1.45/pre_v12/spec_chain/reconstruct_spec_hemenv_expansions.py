from pathlib import Path
import struct,hashlib,math
MASK=0xffffffff
SROT=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
K=[int(abs(math.sin(i+1))*(1<<32)) & MASK for i in range(64)]
def rol(x,n): return ((x<<n)|(x>>(32-n)))&MASK
def transform_md5(state,block):
 a,b,c,d=state; M=list(struct.unpack('<16I',block)); A,B,C,D=a,b,c,d
 for i in range(64):
  if i<16:F=(B&C)|((~B)&D);g=i
  elif i<32:F=(D&B)|((~D)&C);g=(5*i+1)%16
  elif i<48:F=B^C^D;g=(3*i+5)%16
  else:F=C^(B|(~D));g=(7*i)%16
  F=(F+A+K[i]+M[g])&MASK;A,D,C,B=D,C,B,(B+rol(F,SROT[i]))&MASK
 return [(a+A)&MASK,(b+B)&MASK,(c+C)&MASK,(d+D)&MASK]
def dxbc_checksum(data):
 p=data[0x14:];size=len(p);nbits=size*8;state=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
 full=size&~63
 for o in range(0,full,64):state=transform_md5(state,p[o:o+64])
 rem=p[full:]
 if len(rem)>=56:
  block=rem+b'\x80'+b'\0'*(63-len(rem));state=transform_md5(state,block)
  M=[0]*16;M[0]=nbits;M[15]=(nbits>>2)|1;state=transform_md5(state,struct.pack('<16I',*M))
 else:
  buf=bytearray(64);struct.pack_into('<I',buf,0,nbits);buf[4:4+len(rem)]=rem;buf[4+len(rem)]=0x80;struct.pack_into('<I',buf,60,(nbits>>2)|1);state=transform_md5(state,bytes(buf))
 return struct.pack('<4I',*state)

def parse_pe(b):
 pe=struct.unpack_from('<I',b,0x3c)[0];n=struct.unpack_from('<H',b,pe+6)[0];optsz=struct.unpack_from('<H',b,pe+20)[0];opt=pe+24;so=opt+optsz
 secs=[]
 for i in range(n):
  o=so+i*40;name=b[o:o+8].rstrip(b'\0').decode(errors='replace');vs,va,rs,ro=struct.unpack_from('<IIII',b,o+8);ch=struct.unpack_from('<I',b,o+36)[0]
  secs.append((name,vs,va,rs,ro,ch,o))
 return pe,opt,so,secs

def va_to_off(b, absva):
 pe,opt,so,secs=parse_pe(b); base=struct.unpack_from('<Q',b,opt+24)[0];rva=absva-base
 for name,vs,va,rs,ro,ch,o in secs:
  if va<=rva<va+max(vs,rs): return ro+(rva-va)
 raise ValueError(hex(absva))

def patch_dxbc(old:bytes,dcl_word:int,samp_word:int):
 assert old[:4]==b'DXBC'
 total=struct.unpack_from('<I',old,24)[0];cnt=struct.unpack_from('<I',old,28)[0];offs=list(struct.unpack_from('<%dI'%cnt,old,32))
 shex_i=None
 for i,o in enumerate(offs):
  if old[o:o+4] in (b'SHEX',b'SHDR'): shex_i=i;break
 assert shex_i is not None
 o=offs[shex_i]; sz=struct.unpack_from('<I',old,o+4)[0];data=old[o+8:o+8+sz]
 words=list(struct.unpack('<%dI'%(len(data)//4),data))
 # audit word positions are in SHEX data dwords, apparently 0-based
 # inspect expected dcl/sample
 print('at',dcl_word,[hex(x) for x in words[dcl_word:dcl_word+4]],'sample',[hex(x) for x in words[samp_word:samp_word+11]])
 dcl_word=dcl_word-1
 dcl=words[dcl_word:dcl_word+4]
 sample=words[samp_word:samp_word+11]
 assert dcl[3]==1, (dcl_word,dcl)
 assert sample[8]==1, (samp_word,sample)
 ndcl=dcl.copy();ndcl[3]=10
 ns=sample.copy();ns[3]=ns[3]-0x10
 ns[8]=10
 # insert declaration directly after original dcl, sample directly after original sample but account dcl shift
 w=words[:dcl_word+4]+ndcl+words[dcl_word+4:]
 samp2=samp_word+4
 w=w[:samp2+11]+ns+w[samp2+11:]
 # token count is word1; should +15
 assert w[1]==words[1]
 w[1]+=15
 newdata=struct.pack('<%dI'%len(w),*w)
 assert len(newdata)==len(data)+60
 # build chunks, update shex chunk size and following chunk offsets (+60), total size
 buf=bytearray(old)
 # easiest insert 60 bytes at end shex data by replacing chunk bytes with newdata; shifts suffix
 newchunk=old[o:o+4]+struct.pack('<I',len(newdata))+newdata
 before=old[:o]; after=old[o+8+sz:]
 out=bytearray(before+newchunk+after)
 # offsets table: chunks after shex +60
 for i,co in enumerate(offs):
  if co>o: struct.pack_into('<I',out,32+4*i,co+60)
 struct.pack_into('<I',out,24,len(out))
 out[4:20]=b'\0'*16
 out[4:20]=dxbc_checksum(bytes(out))
 return bytes(out)

def sha(b):return hashlib.sha256(b).hexdigest()
if __name__=='__main__':
 base=Path('/mnt/data/spec_chain/ex_tel/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_TELEMETRY.addon64').read_bytes()
 targets={30:(0x100040,49,1119,'8f1e2f66571f2be5b5a0fd66f99405b61ea9da268beda381764cc4db7499dc96'),31:(0x100050,49,1028,'bdd7211358ed826f42a65cd274170b9469c8a9ea32dcbaa638af0e6f065390d6'),32:(0x100060,46,684,'192b13fab9ee23658a6cba9f7f2e42a8f3685e4c8dc3fd3f7772ba4b3ccde9b5')}
 for idx,(rec,dcl,samp,exp) in targets.items():
  va,sz=struct.unpack_from('<QI',base,rec);fo=va_to_off(base,va);old=base[fo:fo+sz];new=patch_dxbc(old,dcl,samp);print(idx,len(old),len(new),sha(new),exp,sha(new)==exp)

def add_section_with_payloads(base:bytes,name:str,rva:int,raw_off:int,raw_size:int,virtual_size:int,payload_specs, size_of_image:int, pointer_updates):
    b=bytearray(base)
    pe,opt,so,secs=parse_pe(b)
    n=len(secs)
    assert len(b)==raw_off,(hex(len(b)),hex(raw_off))
    # section header
    h=so+n*40
    assert all(x==0 for x in b[h:h+40]), b[h:h+40].hex()
    nm=name.encode()[:8].ljust(8,b'\0')
    b[h:h+8]=nm
    struct.pack_into('<IIIIIIHHI',b,h+8,virtual_size,rva,raw_size,raw_off,0,0,0,0,0x40000040)
    struct.pack_into('<H',b,pe+6,n+1)
    struct.pack_into('<I',b,opt+8,struct.unpack_from('<I',b,opt+8)[0]+raw_size) # SizeOfInitializedData
    struct.pack_into('<I',b,opt+56,size_of_image)
    # PE checksum zero
    struct.pack_into('<I',b,opt+64,0)
    sec=bytearray(raw_size)
    for off,payload in payload_specs:
        sec[off:off+len(payload)]=payload
    b+=sec
    imagebase=struct.unpack_from('<Q',b,opt+24)[0]
    for rec_off,sec_off,payload in pointer_updates:
        struct.pack_into('<QI',b,rec_off,imagebase+rva+sec_off,len(payload))
    return bytes(b)

def build_lit_full():
    base=Path('/mnt/data/spec_chain/ex_tel/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_TELEMETRY.addon64').read_bytes()
    info=[(30,0x100040,49,1119,0x0),(31,0x100050,49,1028,0x4fb0),(32,0x100060,46,684,0x9e40)]
    ps=[];ptr=[]
    for idx,rec,dcl,samp,off in info:
        va,sz=struct.unpack_from('<QI',base,rec); old=base[va_to_off(base,va):va_to_off(base,va)+sz]; new=patch_dxbc(old,dcl,samp); ps.append((off,new));ptr.append((rec,off,new))
    out=add_section_with_payloads(base,'.srgblt',0x115000,0x10fa00,0xe800,0xe6f8,ps,0x124000,ptr)
    Path('/mnt/data/spec_chain/LIT_REBUILT.addon64').write_bytes(out)
    exp=Path('/mnt/data/spec_chain/ex_lit/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_LIT_EXPANSION_RC.addon64').read_bytes()
    print('FULL LIT',len(out),sha(out),sha(exp),sha(out)==sha(exp),'diff',sum(a!=b for a,b in zip(out,exp)), 'lenexp',len(exp))
    if out!=exp:
        rs=[];st=None
        for i,(a,bv) in enumerate(zip(out,exp)):
            if a!=bv:
                if st is None:st=i;last=i
                elif i==last+1:last=i
                else:rs.append((st,last));st=last=i
        if st is not None:rs.append((st,last))
        print('ranges',[(hex(a),hex(z),z-a+1,out[a:z+1].hex(),exp[a:z+1].hex()) for a,z in rs[:30]])

if __name__=='__main__':
 print('---full---');build_lit_full()

def build_stage_add(base_path,exp_path,name,rva,raw_off,raw_size,vs,soi,infos,outpath):
    base=Path(base_path).read_bytes(); ps=[];ptr=[]
    for idx,rec,dcl,samp,off,expected_sha in infos:
        va,sz=struct.unpack_from('<QI',base,rec); fo=va_to_off(base,va); old=base[fo:fo+sz]; new=patch_dxbc(old,dcl,samp)
        assert sha(new)==expected_sha,(idx,sha(new),expected_sha)
        ps.append((off,new));ptr.append((rec,off,new))
    out=add_section_with_payloads(base,name,rva,raw_off,raw_size,vs,ps,soi,ptr)
    Path(outpath).write_bytes(out); exp=Path(exp_path).read_bytes()
    print('STAGE',name,sha(out),sha(exp),out==exp,'diff',sum(a!=bv for a,bv in zip(out,exp)),len(out),len(exp))
    return out

def extend_last_section(base:bytes,section_name:str,new_vs:int,new_rs:int,new_soi:int,infos):
    b=bytearray(base); pe,opt,so,secs=parse_pe(b); sec=[s for s in secs if s[0]==section_name][0]; name,oldvs,rva,oldrs,ro,ch,h=sec
    assert len(b)==ro+oldrs,(hex(len(b)),hex(ro+oldrs)); assert new_rs>=oldrs
    ext=bytearray(new_rs-oldrs); imagebase=struct.unpack_from('<Q',b,opt+24)[0]
    for idx,rec,dcl,samp,off,expected_sha in infos:
        va,sz=struct.unpack_from('<QI',b,rec); fo=va_to_off(b,va); old=bytes(b[fo:fo+sz]); new=patch_dxbc(old,dcl,samp); assert sha(new)==expected_sha,(idx,sha(new),expected_sha)
        assert off>=oldrs; eo=off-oldrs; ext[eo:eo+len(new)]=new
        struct.pack_into('<QI',b,rec,imagebase+rva+off,len(new))
    b+=ext
    struct.pack_into('<I',b,h+8,new_vs); struct.pack_into('<I',b,h+16,new_rs)
    struct.pack_into('<I',b,opt+8,struct.unpack_from('<I',b,opt+8)[0]+(new_rs-oldrs))
    struct.pack_into('<I',b,opt+56,new_soi); struct.pack_into('<I',b,opt+64,0)
    return bytes(b)

def run_all():
    lit='/mnt/data/spec_chain/LIT_REBUILT.addon64'
    mulinfos=[
    (27,0x100010,55,1302,0x0,'512aad558517b8e2fee9cf4cc2d9ae470476079a77f185528bbbc2cf36b82835'),
    (28,0x100020,55,1211,0x5350,'429b01a75974d479dafb848c66b0fb1c974ee8bc718c8cb2451a3336adfb6a1a'),
    (29,0x100030,52,871,0xa570,'2615a708f15a9b12735ab9a52dc7aefbb6677fbcef9549e92fd1d8048077b829')]
    mulout='/mnt/data/spec_chain/MUL_REBUILT.addon64'
    build_stage_add(lit,'/mnt/data/spec_chain/ex_mul/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_MUL_EXPANSION_RC.addon64','.srgbml',0x124000,0x11e200,0xf200,0xf1b0,0x134000,mulinfos,mulout)
    mtinfos=[
    (24,0xfffe0,58,1364,0x0,'f830841c7d94b69eee690787ef8365195a91dc5faabc79a0a40818d811e178f9'),
    (25,0xffff0,58,1273,0x5540,'4c78fae210d1ea0081247a46495e4603d427344f3e27353b945782b3d8e91e1a'),
    (26,0x100000,55,929,0xa950,'fe9998d398dbf47e6378dc54e5ece3775669c6951bad73b7be3f182581aa6862')]
    mtout='/mnt/data/spec_chain/MULLIT_REBUILT.addon64'
    build_stage_add(mulout,'/mnt/data/spec_chain/ex/mullit/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_MULLIT_EXPANSION_RC.addon64','.srgbmt',0x134000,0x12d400,0xf800,0xf794,0x144000,mtinfos,mtout)
    fullinfos=[
    (36,0x1000a0,52,920,0xf800,'9b0527e4b794a93e10bbbefb23819a6acd2eac056192033f76bf22619846f538'),
    (37,0x1000b0,52,829,0x14550,'6173054e9ddde73b6c5a6cc2551b6d5ed16e2bc8622bee7e65a2023028629f47'),
    (38,0x1000c0,49,485,0x19170,'8cb7d6ce1bab37548795d28a444ff0e7301f3eecbad315af5f7680d68880f2c1'),
    (39,0x1000d0,49,858,0x1d7c0,'1cc5b6dd4f3315387ee35f0e738180df46fb940aadce5724743829d4abda8809'),
    (40,0x1000e0,49,767,0x22310,'4bdeb3240b4675d7d95443c0e6d7d78cbac6389d39e22728912e8284bfed85dc'),
    (41,0x1000f0,46,427,0x26d30,'11af2be623892ff349819d6e187e62c5937e37e266c57088ced0b097ec9abce2'),
    (42,0x100100,46,862,0x2b180,'f668f84ae3687587caa4aab6fd9937ebdf4e791f2b1a39a8260d0b97cbf0f213'),
    (43,0x100110,46,771,0x2fcb0,'4f8bfabe4d842cd4864c8ea3647178170639dd41924f2bd6cccf70c61b62957d'),
    (44,0x100120,43,427,0x346b0,'32daa23d2aafbb16228e4faba35e6c2d822cb3e6ddb873fa19599f16bfcc56e2'),
    (45,0x100130,43,803,0x38ae0,'47602d7439f8a0d3d4603991c2d39967aac8460cdccddeb3eb39b5dd357eb3bf'),
    (46,0x100140,43,712,0x3d440,'bd7245aa5296e82abd601d8776d0ba9e2b64e4b58f4dbf16b1fb47c4b75e96ab'),
    (47,0x100150,40,372,0x41c70,'0a8343fe58ce37dd266612946d4b9ffc7c8786354efcad2644434ecd32d89740')]
    b=Path(mtout).read_bytes();out=extend_last_section(b,'.srgbmt',0x45ec0,0x46000,0x17a000,fullinfos); Path('/mnt/data/spec_chain/FULL24_REBUILT.addon64').write_bytes(out)
    exp=Path('/mnt/data/spec_chain/ex/full24/DSRRL_Material_Response_1.3_PTDE_SPEC_BRIDGE_FULL24_STABLE_HEMENV_RC.addon64').read_bytes(); print('FULL24',sha(out),sha(exp),out==exp,'diff',sum(a!=bv for a,bv in zip(out,exp)),len(out),len(exp))
    if out!=exp:
      rs=[];st=None
      for i,(a,bv) in enumerate(zip(out,exp)):
        if a!=bv:
          if st is None:st=last=i
          elif i==last+1:last=i
          else:rs.append((st,last));st=last=i
      if st is not None:rs.append((st,last))
      print('FULLDIFF',[(hex(a),hex(z),z-a+1,out[a:z+1].hex(),exp[a:z+1].hex()) for a,z in rs[:20]])

if __name__=='__main__':
 print('---all---');run_all()
