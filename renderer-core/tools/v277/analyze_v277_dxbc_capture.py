#!/usr/bin/env python3
"""Read-only V277 DXBC capture digest/SM5 opcode census. Never infers GPU pixels."""
import argparse,csv,hashlib,json,re,struct
from collections import Counter,defaultdict
from pathlib import Path
def decode(blob):
    if len(blob)<40 or blob[:4]!=b"DXBC":raise ValueError("invalid DXBC magic")
    _,size,n=struct.unpack_from("<III",blob,20)
    if size!=len(blob) or not 1<=n<=48:raise ValueError("DXBC size/count")
    offsets=struct.unpack_from("<%dI"%n,blob,32)
    for off in offsets:
        if off+8>len(blob):raise ValueError("chunk header outside DXBC")
        tag=blob[off:off+4];size=struct.unpack_from("<I",blob,off+4)[0]
        if off+8+size>len(blob):raise ValueError("chunk overrun")
        if tag not in (b"SHEX",b"SHDR"):continue
        data=blob[off+8:off+8+size]
        if len(data)%4:raise ValueError("unaligned SHEX")
        words=struct.unpack("<%dI"%(len(data)//4),data)
        if len(words)<3 or words[1]!=len(words):raise ValueError("SHEX words count")
        ops=[];i=2
        while i<len(words):
            op=words[i]&0x7ff;n=(words[i]>>24)&0x7f
            if n<1 or i+n>len(words):
                raise ValueError("unsupported DXBC instruction length, do not guess")
            ops.append((op,tuple(words[i:i+n])));i+=n
        return tag.decode(),words[0],ops
    raise ValueError("no supported SHDR/SHEX chunk")
def analyze(folder):
    entries=[];pairs=defaultdict(dict)
    for file in sorted(folder.glob("RX*_*.dxbc")):
        m=re.fullmatch(r"RX(33|34|35)_(STABLE|LERP)_(STOCK|BRIDGE)_([0-9a-fA-F]{16})\.dxbc",file.name)
        if not m:continue
        rx,phase,kind,_=m.groups();blob=file.read_bytes()
        info=dict(name=file.name,rx=int(rx),phase=phase,kind=kind,
                  bytes=len(blob),sha256=hashlib.sha256(blob).hexdigest())
        try:
            chunk,version,ops=decode(blob)
            info.update(chunk=chunk,version=hex(version),instructions=len(ops),
                        opcode_counts=dict(Counter(op for op,_ in ops)),
                        decode="PASS")
        except ValueError as ex:
            info.update(decode="UNSUPPORTED",why=str(ex));ops=None
        key=(phase,int(rx))
        if kind in pairs[key]:raise ValueError("more than one exact shader for "+str(key))
        pairs[key][kind]=(info,ops)
        entries.append(info)
    delta=[]
    for (phase,rx),g in sorted(pairs.items()):
        if set(g)!={"STOCK","BRIDGE"}:continue
        a,ac=g["STOCK"];b,bc=g["BRIDGE"]
        if ac is None or bc is None:continue
        aa=Counter(op for op,_ in ac);bb=Counter(op for op,_ in bc)
        delta.append(dict(phase=phase,rx=rx,stock_sha=a["sha256"],
            bridge_sha=b["sha256"],opcode_added=dict(bb-aa),
            opcode_removed=dict(aa-bb),pixel="OPEN"))
    return dict(files=entries,source_instruction_deltas=delta,
                authority="CREATE_TIME_MATERIALIZER_ONLY",
                gpu_actual_execution="UNVERIFIED",pixel="OPEN")
def main():
    p=argparse.ArgumentParser();p.add_argument("capture_dir",type=Path)
    p.add_argument("--out",type=Path,default=Path("v277_dxbc_census.json"))
    a=p.parse_args();out=analyze(a.capture_dir)
    a.out.write_text(json.dumps(out,indent=2)+"\n")
    print("V277_DXBC_FILES",len(out["files"]),"SOURCE_PAIRS",len(out["source_instruction_deltas"]))
    print("ACTUAL_GPU_EXECUTION_AND_PIXEL_OPEN")
if __name__=="__main__":main()
