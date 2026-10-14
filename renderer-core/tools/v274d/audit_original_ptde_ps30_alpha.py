#!/usr/bin/env python3
"""Reproduce no-live-SpecTex.a PTDE PS3.0 HemEnvLerp census.

Requires owner-provided DSRRL_FINAL_EVIDENCE_PART02 zip; no game assets
are committed or distributed with this tool.
"""
import argparse,hashlib,json,re,struct,zipfile
from pathlib import Path
def reg(x):
    return ((x&0x70000000)>>28)|((x&0x1800)>>8),x&2047
def decode(words):
    p=1
    while p<len(words):
        t=words[p];op=t&65535
        if op==65535:break
        if op==65534:p+=1+((t>>16)&32767);continue
        n=(t>>24)&15
        if not n:raise RuntimeError("Invalid instruction length")
        yield p,op,words[p+1:p+1+n]
        p+=n+1
def has_sampler1(data):
    at=data.index(b'CTAB')+4
    size,_,ver,n,off,_,_=struct.unpack_from('<7I',data,at)
    if size!=28 or ver!=0xffff0300:raise ValueError("Unexpected CTAB")
    for i in range(n):
        nm,kind,ix,count,_,_,_=struct.unpack_from('<IHHHHII',data,at+off+20*i)
        if kind==3 and ix==1:return data[at+nm:].split(b'\x00',1)[0].decode()
    raise ValueError("Missing sampler1")
def inspect(words):
    ins=list(decode(words));res=[]
    for idx,(p,op,terms) in enumerate(ins):
        if op!=0x42 or len(terms)<3 or reg(terms[-1])!=(10,1):continue
        sampled=reg(terms[0]);read=0;clobbered=False
        for _,nextop,src in ins[idx+1:]:
            if nextop in (0x1f,0x51) or not src:continue
            mask=(src[0]>>16)&15
            for tok in src[1:]:
                if reg(tok)==sampled and any(
                    (((tok>>16)>>(2*lane))&3)==3
                    for lane in range(4) if mask&(1<<lane)):
                    read+=1
            if reg(src[0])==sampled and mask&8:
                clobbered=True;break
        res.append((p,read,clobbered))
    branches=sum(o in (0x28,0x29,0x2a,0x2b) for _,o,_ in ins)
    return res,branches,len(ins)
def main():
    p=argparse.ArgumentParser();p.add_argument('archive',type=Path);p.add_argument('--output',type=Path,default=Path('v274d_ptde_alpha_scan.json'))
    a=p.parse_args();rows=[]
    with zipfile.ZipFile(a.archive) as z:
        targets=sorted(n for n in z.namelist() if '/PTDE_FRPG_PHN_EXTRACTED/' in n and '/FRPG_Phn_DifSpc' in n and n.endswith('HemEnvLerp.fpo'))
        for n in targets:
            data=z.read(n);w=list(struct.unpack('<%dI'%(len(data)//4),data))
            if w[0]!=0xffff0300:raise ValueError('Not PS 3.0')
            sampler=has_sampler1(data);reads,branches,count=inspect(w)
            rows.append(dict(name=n.rsplit('/',1)[-1],sha256=hashlib.sha256(data).hexdigest(),sampler=sampler,s1_samples=len(reads),alpha_live_reads=sum(x[1] for x in reads),alpha_clobbered=[x[2] for x in reads],branches=branches,instructions=count))
    assert len(rows)==24 and all(r['s1_samples']==1 and r['alpha_live_reads']==0 and r['branches']==0 for r in rows)
    print('24 original PTDE shaders, 24 s1 samples, zero live alpha reads; no shader binary written.')
    a.output.write_text(json.dumps({'archive_sha256':hashlib.sha256(a.archive.read_bytes()).hexdigest(),'scope':'PTDE PHN DifSpc/DifSpcBmp HemEnvLerp no PointLight 24 exact shader variants','gpu_execution':'UNVERIFIED','pixel':'OPEN','rows':rows},indent=2)+'\n')
if __name__=='__main__':main()
