from pathlib import Path
import struct, hashlib, json
from importlib.machinery import SourceFileLoader
D=SourceFileLoader('d','/mnt/data/dxbc_diff.py').load_module()
C=SourceFileLoader('c','/mnt/data/dxbc_checksum.py').load_module()
R=Path('/mnt/data/full24_work')
CASES=[
 ('Csd', R/'FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf_T13_SPEC.fpo', R/'FRPG_Phn_DifSpcBmp______Csd_HemEnvSubsurf_SPEC_ONLY_T13.fpo'),
 ('Sdw', R/'FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf_T13_SPEC.fpo', R/'FRPG_Phn_DifSpcBmp______Sdw_HemEnvSubsurf_SPEC_ONLY_T13.fpo'),
 ('plain', R/'FRPG_Phn_DifSpcBmp__________HemEnvSubsurf_T13_SPEC.fpo', R/'FRPG_Phn_DifSpcBmp__________HemEnvSubsurf_SPEC_ONLY_T13.fpo'),
]

def split_chunks(b):
    assert b[:4]==b'DXBC'
    n=struct.unpack_from('<I',b,28)[0]
    offs=list(struct.unpack_from('<%dI'%n,b,32))
    out=[]
    for o in offs:
        tag=b[o:o+4]
        sz=struct.unpack_from('<I',b,o+4)[0]
        out.append((tag,b[o+8:o+8+sz]))
    return out

def rebuild_with_shex(b,new_shex):
    chunks=split_chunks(b)
    chunks=[(tag,new_shex if tag==b'SHEX' else data) for tag,data in chunks]
    n=len(chunks); hdr_len=32+4*n
    hdr=bytearray(b[:hdr_len]); hdr[4:20]=b'\0'*16
    offs=[]; body=bytearray()
    cur=hdr_len
    for tag,data in chunks:
        assert cur%4==0
        offs.append(cur)
        ch=tag+struct.pack('<I',len(data))+data
        body.extend(ch); cur+=len(ch)
        if cur%4:
            pad=4-cur%4;body.extend(b'\0'*pad);cur+=pad
    out=hdr+body
    struct.pack_into('<I',out,24,len(out)); struct.pack_into('<I',out,28,n)
    struct.pack_into('<%dI'%n,out,32,*offs)
    return C.patch(bytes(out))

def is_cb79(ws):
    return any(ws[i:i+3]==[0x0020803a,0,79] for i in range(len(ws)-2))

def resource_index(ws,idx):
    return any((ws[i]&0x00fff000)==0x00107000 and ws[i+1]==idx for i in range(len(ws)-1))

def sampler_index(ws,idx):
    return any((ws[i]&0x00fff000)==0x00106000 and ws[i+1]==idx for i in range(len(ws)-1))

def get_isgn_color0_reg(b):
    ch=dict(split_chunks(b)); s=ch[b'ISGN']; cnt=struct.unpack_from('<I',s,0)[0]
    for i in range(cnt):
        o=8+i*24; no,semidx,sv,ct,reg=struct.unpack_from('<5I',s,o)
        end=s.find(b'\0',no); name=s[no:end].decode('ascii',errors='replace')
        if name=='COLOR' and semidx==0: return reg
    raise RuntimeError('COLOR0 not found')

def decl_index(ws):
    # DCL sampler/resource operand is words1,2
    return ws[2] if len(ws)>=3 else None

def transform(inp:bytes):
    w,ins=D.insts(inp)
    inst=[list(x[2]) for x in ins]
    color_reg=get_isgn_color0_reg(inp)
    # declarations: add b12, repurpose s9/t9 -> s14/t14, add one fresh temp
    cb1=[i for i,x in enumerate(ins) if D.opname(x[1])=='DCL_CONSTANT_BUFFER' and x[2][2]==1]
    assert len(cb1)==1
    insert_decl=cb1[0]+1
    inst.insert(insert_decl,[0x04000059,0x00208e46,12,4])
    # work on current list using opcodes directly
    def opname(ws): return D.opname(ws[0]&0x7ff)
    s9=[]; t9=[]; temps=[]
    for i,ws in enumerate(inst):
        if opname(ws)=='DCL_SAMPLER' and len(ws)>=3 and ws[2]==9: s9.append(i)
        if opname(ws)=='DCL_RESOURCE' and len(ws)>=3 and ws[2]==9: t9.append(i)
        if opname(ws)=='DCL_TEMPS': temps.append(i)
    assert len(s9)==1 and len(t9)==1 and len(temps)==1,(s9,t9,temps)
    inst[s9[0]][2]=14; inst[t9[0]][2]=14
    scratch=inst[temps[0]][1]
    inst[temps[0]][1]=scratch+1
    # domain: t13 SAMPLE, then MUL, LOG, MUL(2.2), EXP -> retain SAMPLE+MUL then MOV final=mul result
    t13samples=[]
    for i,ws in enumerate(inst):
        if opname(ws)=='SAMPLE' and resource_index(ws,13): t13samples.append(i)
    assert len(t13samples)==1,t13samples
    ti=t13samples[0]
    assert [opname(inst[ti+j]) for j in range(1,5)]==['MUL','LOG','MUL','EXP']
    mulws=inst[ti+1]; logws=inst[ti+2]; expws=inst[ti+4]
    # verify stock pow2.2 immediate in middle MUL
    assert 0x400ccccd in inst[ti+3]
    mov=[0x05000036]+expws[1:3]+logws[3:5]
    inst[ti+2:ti+5]=[mov]
    # material response anchor
    anchors=[]
    for i,ws in enumerate(inst):
        if opname(ws)=='ADD' and is_cb79(ws) and 0xbf800000 in ws: anchors.append(i)
    assert len(anchors)==1,anchors
    a=anchors[0]
    seq=[opname(x) for x in inst[a:a+15]]
    assert seq[:14]==['ADD','LOG','MAD','ADD','MUL','SAMPLE_L','MUL','DP3','MAD','MUL','MUL','SAMPLE_L','MUL','MAD'],seq
    log0=inst[a+1]
    mad=inst[a+2].copy(); add=inst[a+3].copy(); oldmul=inst[a+4].copy(); cube=inst[a+5].copy(); scalemul=inst[a+6].copy(); oldmad=inst[a+13].copy()
    # MAD final source -> cb0[79].w
    assert len(mad)==9
    mad=mad[:-2]+[0x0020803a,0,79]; mad[0]=(mad[0]&0x00ffffff)|(len(mad)<<24)
    # -2 -> -3
    hits=[i for i,x in enumerate(add) if x==0xc0000000]; assert len(hits)==1
    add[hits[0]]=0xc0400000
    # clamp LOD-ish coordinate with b12[0..1] using same dest/source operand as ADD
    dest=add[1:3]; src=add[3:5]
    mx=[0x08000034]+dest+src+[0x0020803a,12,0]
    mn=[0x08000033]+dest+src+[0x0020803a,12,1]
    # scale cube contribution by b12[2]
    assert len(scalemul)==7
    scalemul=scalemul[:5]+[0x00208246,12,2]; scalemul[0]=(scalemul[0]&0x00ffffff)|(len(scalemul)<<24)
    # original t12 sample's destination/coords/lod define branch interpolation coordinates
    assert opname(cube)=='SAMPLE_L' and resource_index(cube,12)
    orig_reg=cube[4]; coord=cube[5:7]; lod=cube[11:13]
    sample14=[0x8d000048,0x80000182,0x00155543,0x001000e2,scratch]+coord+[0x00107936,14,0x00106000,14]+lod
    iff=[0x0404001f,0x0020803a,12,3]
    mad_scratch=[0x0b000032,0x001000e2,scratch,0x00100e56,scratch,0x00208246,12,3,0x80100e56,0x41,orig_reg]
    mad_orig=[0x0a000032,0x001000e2,orig_reg,0x00100e56,scratch,0x0020803a,12,3,0x00100e56,orig_reg]
    endif=[0x01000015]; nop=[0x0100003a]
    # collapse stock t9 spec calculation into explicit PTDE c101 multiplier and COLOR0
    assert opname(oldmad)=='MAD' and len(oldmad)==9
    outdest=oldmad[1:3]; firstsrc=oldmad[3:5]
    c101mul=[0x08000038]+outdest+firstsrc+[0x00208246,12,0]
    # source1 needs source selector for same output register; use current plain canonical selector 0x00100246
    outreg=outdest[1]
    color_mul=[0x07000038]+outdest+[0x00100246,outreg,0x00101246,color_reg]
    replacement=[
        log0,mad,add,mx,mn,cube,scalemul,iff,sample14,mad_scratch,mad_orig,endif,nop.copy(),nop.copy(),
        c101mul,color_mul,nop.copy(),nop.copy()
    ]
    # original consumed a..a+14, retaining original instruction a+14 onward
    inst[a:a+14]=replacement
    # declarations sanity after replacement: no sample/resource/sampler t9, one t14 sample; b12 exists
    # build SHEX words
    old_shex=dict(split_chunks(inp))[b'SHEX']; sw=list(struct.unpack('<%dI'%(len(old_shex)//4),old_shex))
    shex=[sw[0],0]
    for ws in inst: shex.extend(ws)
    shex[1]=len(shex)
    new_shex=struct.pack('<%dI'%len(shex),*shex)
    out=rebuild_with_shex(inp,new_shex)
    # audit parse
    nw,ni=D.insts(out); names=[D.opname(x[1]) for x in ni]
    def count_seq(seq): return sum(1 for i in range(len(nw)-len(seq)+1) if nw[i:i+len(seq)]==seq)
    refs=lambda idx: sum(1 for i in range(len(nw)-1) if (nw[i]&0x00fff000)==0x00107000 and nw[i+1]==idx)
    samrefs=lambda idx: sum(1 for i in range(len(nw)-1) if (nw[i]&0x00fff000)==0x00106000 and nw[i+1]==idx)
    # DCL / samples
    t13_sample=sum(1 for _,op,ws in ni if D.opname(op)=='SAMPLE' and resource_index(ws,13))
    t10_samples=sum(1 for _,op,ws in ni if D.opname(op).startswith('SAMPLE') and resource_index(ws,10))
    t14_samples=sum(1 for _,op,ws in ni if D.opname(op)=='SAMPLE_L' and resource_index(ws,14))
    t9_samples=sum(1 for _,op,ws in ni if D.opname(op).startswith('SAMPLE') and resource_index(ws,9))
    cb12_0=count_seq([0x00208246,12,0])+count_seq([0x0020803a,12,0])
    cb12_1=count_seq([0x0020803a,12,1])
    cb12_2=count_seq([0x00208246,12,2])
    cb12_3=count_seq([0x0020803a,12,3])+count_seq([0x00208246,12,3])
    pow22_after_t13=False
    # locate t13 sample in transformed payload and check nearby five instructions contains no LOG/MUL2.2/EXP chain
    ii=[i for i,x in enumerate(ni) if D.opname(x[1])=='SAMPLE' and resource_index(x[2],13)][0]
    near=ni[ii:ii+5]
    if len(near)>=5:
        pow22_after_t13=[D.opname(x[1]) for x in near[2:5]]==['LOG','MUL','EXP']
    dcl=lambda op,idx: sum(1 for _,opc,ws in ni if D.opname(opc)==op and len(ws)>=3 and ws[2]==idx)
    non_shex_equal=all(a==b for (ta,a),(tb,b) in zip(split_chunks(inp),split_chunks(out)) if ta!=b'SHEX' and tb==ta)
    info=dict(color0_reg=color_reg,scratch_temp=scratch,new_temp_count=scratch+1,t13_sample=t13_sample,t10_samples=t10_samples,t14_samples=t14_samples,t9_samples=t9_samples,
              b12_decl=dcl('DCL_CONSTANT_BUFFER',12),t10_decl=dcl('DCL_RESOURCE',10),t13_decl=dcl('DCL_RESOURCE',13),t14_decl=dcl('DCL_RESOURCE',14),s14_decl=dcl('DCL_SAMPLER',14),s9_decl=dcl('DCL_SAMPLER',9),
              cb12_0_refs=cb12_0,cb12_1_refs=cb12_1,cb12_2_refs=cb12_2,cb12_3_refs=cb12_3,pow22_after_t13=pow22_after_t13,non_shex_byte_identical=non_shex_equal,
              checksum='PASS' if out[4:20]==C.dxbc_checksum(out) else 'FAIL',size=len(out),sha256=hashlib.sha256(out).hexdigest())
    assert info['checksum']=='PASS'
    assert t13_sample==1 and t10_samples>=1 and t14_samples==1 and t9_samples==0,info
    assert info['b12_decl']==1 and info['t10_decl']>=1 and info['t13_decl']==1 and info['t14_decl']==1 and info['s14_decl']==1 and info['s9_decl']==0 and info['non_shex_byte_identical'],info
    assert cb12_0>=2 and cb12_1>=1 and cb12_2>=1 and cb12_3>=2,info
    assert not pow22_after_t13
    # RDEF/ISGN/OSGN/STAT remain byte-identical; t10 sample retention below is the direct SSS resource invariant.
    assert all(a==b for (ta,a),(tb,b) in zip(split_chunks(inp),split_chunks(out)) if ta!=b'SHEX' and tb==ta)
    return out,info

report=[]
for variant,src,dst in CASES:
    inp=src.read_bytes(); out,info=transform(inp); dst.write_bytes(out)
    info.update(variant=variant,source=src.name,output=dst.name,source_sha256=hashlib.sha256(inp).hexdigest())
    report.append(info)
Path('/mnt/data/subsurf_spec_only_receiver_prebuild_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
