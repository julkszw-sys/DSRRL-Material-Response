#!/usr/bin/env python3
"""Original owner DXBC branch audit: exact b0[85].x IF true/ELSE in RX33-35.
Source-only, no GPU CB readback; original DXBC remains owner-private.
Usage: python v279_lerp_branch_semantic_census.py DXBC_CAPTURE_DIR --out r.json
"""
import argparse,struct,hashlib,json
from pathlib import Path
from difflib import SequenceMatcher

def parse(path):
    b=path.read_bytes()
    assert b[:4]==b'DXBC' and len(b)>=40
    n=struct.unpack_from('<I',b,28)[0]
    off=struct.unpack_from('<%dI'%n,b,32)
    chunks={b[i:i+4]:(i,struct.unpack_from('<I',b,i+4)[0]) for i in off}
    p,length=chunks.get(b'SHEX',chunks.get(b"SHDR"))
    w=struct.unpack_from('<%dI'%(length//4),b,p+8)
    assert w[1]==len(w)
    i=2;seq=[]
    while i<len(w):
        op=w[i]&0x7ff;length=(w[i]>>24)&0x7f
        assert length>=1 and i+length<=len(w)
        seq.append((op,tuple(w[i:i+length])));i+=length
    return b,seq

def split(seq):
    f=[j for j,(_,v) in enumerate(seq)
       if v==(0x0400001f,0x0020800a,0,85)]
    assert len(f)==1
    start=f[0];depth=0;other=None;end=None
    for j in range(start+1,len(seq)):
        op=seq[j][0]
        if op==31:depth+=1
        elif op==21:
            if depth==0:end=j;break
            depth-=1
        elif op==18 and depth==0:
            assert other is None
            other=j
    assert other is not None and end is not None
    return {'true':seq[start+1:other], 'else':seq[other+1:end]}

def changes(stock,bridge):
    sm=SequenceMatcher(None,[t for _,t in stock],[t for _,t in bridge],
                       autojunk=False)
    ans=[]
    for action,i,j,p,q in sm.get_opcodes():
        if action=='equal':continue
        assert action=='replace' and j-i==1 and q-p==1
        a,b=stock[i][1],bridge[p][1]
        ans.append({'opcode':a[0]&0x7ff,'stock_tokens':list(map(hex,a)),
                    'bridge_tokens':list(map(hex,b))})
    return ans

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('capture_dir',type=Path)
    ap.add_argument('--out',type=Path,default=Path('v279_lerp_branch_audit.json'))
    a=ap.parse_args()
    result=[]
    for rx in (33,34,35):
        v={}
        for kind in ('STOCK','BRIDGE'):
            files=list(a.capture_dir.glob(f'RX{rx}_LERP_{kind}_*.dxbc'))
            assert len(files)==1,('missing/ambiguous RX',rx,kind,files)
            blob,ins=parse(files[0])
            v[kind]=(hashlib.sha256(blob).hexdigest(),split(ins))
        st,sw=v['STOCK'];br,bw=v['BRIDGE']
        true=changes(sw['true'],bw['true'])
        alternate=changes(sw['else'],bw['else'])
        assert len(true)==4 and len(alternate)==2
        result.append(dict(rx=rx,stock_sha256=st,bridge_sha256=br,
                gate='exact DXBC IF b0[85].x',gate_gpu_value='NOT_READ',
                modifications_true=true,modifications_else=alternate,
                cpu_b0_85='ZERO_IN_OWNER_CPU_SHADOW_ONLY',
                ptde_pixel='UNOBSERVED'))
    a.out.write_text(json.dumps(result,indent=2)+'\n')
    print('V279_SOURCE_BRANCH_3RX_PASS true=4 else=2')
    print('GPU_ACTUAL_BRANCH=OPEN PIXEL=OPEN')
if __name__=='__main__':main()
