from __future__ import annotations
from pathlib import Path
import sys, json, hashlib, struct, argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from generate_port_plans import parse_bnd3, code_info, instrs, dxbc_chunks, fix_checksum
B13_DECL=[0x04000059,0x00208e46,0x0000000d,0x00000008]

def rebuild(data:bytes,new_code_payload:bytes)->bytes:
    chunks=[]
    for _,t,p in dxbc_chunks(data):
        if t==b'RDEF':
            continue
        chunks.append((t,new_code_payload if t in (b'SHEX',b'SHDR') else p))
    n=len(chunks); header_size=32+4*n; offs=[]; body=bytearray(); cur=header_size
    for t,p in chunks:
        offs.append(cur); ch=t+struct.pack('<I',len(p))+p; body+=ch; cur+=len(ch)
    out=bytearray(data[:20]); out+=struct.pack('<I',1); out+=struct.pack('<I',cur); out+=struct.pack('<I',n); out+=struct.pack('<'+'I'*n,*offs); out+=body
    out[4:20]=b'\0'*16
    return fix_checksum(bytes(out))

def cbrefs(ins):
    out=[]; i=1
    while i<len(ins):
        tok=ins[i]; base=tok & 0x7fffffff
        if (base & 0x00fff00f)==0x00208006:
            ext=bool(tok&0x80000000); j=i+1+(1 if ext else 0)
            if j+1<len(ins): out.append((i,j,j+1,ins[j],ins[j+1],ext))
            i=j+2; continue
        i+=1
    return out

def patch_ul(base:bytes,label:str):
    _,w=code_info(base); aa=instrs(w)
    candidates=[]
    for i in range(1,len(aa)-1):
        prev,mid,nxt=aa[i-1],aa[i],aa[i+1]
        if prev[1]!=0x32 or mid[1]!=0x0 or nxt[1]!=0x32: continue
        mr=cbrefs(mid[2]); nr=cbrefs(nxt[2])
        sm=[(r[3],r[4]) for r in mr]; sn=[(r[3],r[4]) for r in nr]
        if (0,7) in sm and (0,8) in sm and (0,8) in sn: candidates.append((i,mr,nr))
    if len(candidates)!=1: raise AssertionError((label,'UL island candidates',len(candidates)))
    i,mr,nr=candidates[0]; changed=[]
    for which,refs,ins_off in [('ADD',mr,aa[i][0]),('MAD',nr,aa[i+1][0])]:
        for r in refs:
            slot_idx,vec_idx=r[1],r[2]; slot,vec=r[3],r[4]
            if slot!=0 or vec not in (7,8): continue
            newvec=6 if vec==7 else 7
            w[ins_off+slot_idx]=13; w[ins_off+vec_idx]=newvec
            changed.append({'instruction':which,'old_cb':0,'old_index':vec,'new_cb':13,'new_index':newvec})
    if len(changed)!=3: raise AssertionError((label,'expected 3 U/L refs',changed))
    aa2=instrs(w); dcls=[x for x in aa2 if x[1]==0x59]
    if not dcls: raise AssertionError((label,'no CB declaration'))
    last=dcls[-1]; insert_at=last[0]+len(last[2])
    nw=w[:insert_at]+B13_DECL+w[insert_at:]; nw[1]=len(nw)
    payload=struct.pack('<'+'I'*len(nw),*nw); out=rebuild(base,payload)
    tags=[t for _,t,_ in dxbc_chunks(out)]
    if b'RDEF' in tags: raise AssertionError((label,'RDEF retained'))
    _,ww=code_info(out); aaa=instrs(ww)
    b13d=[x for x in aaa if x[1]==0x59 and len(x[2])==4 and x[2][1]==0x00208e46 and x[2][2]==13 and x[2][3]==8]
    if len(b13d)!=1: raise AssertionError((label,'b13 decl count',len(b13d)))
    refs=[]
    for _,op,ins in aaa:
        if op==0x59: continue
        for r in cbrefs(ins):
            if r[3]==13: refs.append(r[4])
    if sorted(refs)!=[6,7,7]: raise AssertionError((label,'b13 refs',refs))
    return out, {'island_instruction_index':i,'changed_refs':changed,'b13_refs':refs,'rdef_stripped':True}

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--binder-bnd',required=True); ap.add_argument('--out-root',default=str(ROOT)); a=ap.parse_args(); outroot=Path(a.out_root)
    rows=parse_bnd3(Path(a.binder_bnd).read_bytes()); byhash={hashlib.sha256(r['data']).hexdigest():r for r in rows}
    pp=json.loads((ROOT/'data/PORT_PLAN.json').read_text()); planby={p['original_sha256']:p for p in pp['plans']}
    hs=json.loads((ROOT/'data/UL48_CANONICAL_HOSTS.json').read_text())['records']; recs=[]; blobs=[]
    for idx,hrec in enumerate(hs):
        h=hrec['original_sha256']; label=hrec['name']; r=byhash[h]; orig=r['data']; pl=planby[h]; p22=bytearray(orig)
        for op in pl['ops']:
            cur=struct.unpack_from('<I',p22,op['byte_offset'])[0]
            if cur!=op['old']: raise AssertionError((label,'P22 guard',op,hex(cur)))
            struct.pack_into('<I',p22,op['byte_offset'],op['new'])
        p22=fix_checksum(bytes(p22)); p22sha=hashlib.sha256(p22).hexdigest()
        if p22sha!=pl['replacement_sha256']: raise AssertionError((label,p22sha,pl['replacement_sha256']))
        ul,aud=patch_ul(p22,label); ulsha=hashlib.sha256(ul).hexdigest(); blobs.append(ul)
        recs.append({'index':idx,'id':r['id'],'name':label,'original_sha256':h,'p22_sha256':p22sha,'ul_sha256':ulsha,'original_bytes':len(orig),'p22_bytes':len(p22),'ul_bytes':len(ul),**aud})
    lines=['#pragma once','#include <array>','#include <cstddef>','#include <cstdint>','#include <string_view>','namespace dsrrl::ul48live {','struct plan { std::string_view label; std::string_view original_sha256; std::string_view p22_sha256; std::string_view ul_sha256; const std::uint8_t *code; std::size_t code_size; };']
    for i,b in enumerate(blobs):
        lines.append(f'inline constexpr std::array<std::uint8_t,{len(b)}> blob_{i} = {{')
        for j in range(0,len(b),24): lines.append('  '+','.join(str(x) for x in b[j:j+24])+',')
        lines.append('};')
    lines.append('inline constexpr std::array<plan,48> k_plans = {{')
    for i,r in enumerate(recs): lines.append(f'  {{"{r["name"]}","{r["original_sha256"]}","{r["p22_sha256"]}","{r["ul_sha256"]}",blob_{i}.data(),blob_{i}.size()}},')
    lines += ['}};','inline constexpr int find_original(std::string_view h) noexcept { for (std::size_t i=0;i<k_plans.size();++i) if(k_plans[i].original_sha256==h) return static_cast<int>(i); return -1; }','} // namespace dsrrl::ul48live']
    (outroot/'include/dsrrl/generated_ul48_live.hpp').write_text('\n'.join(lines)+'\n')
    audit={'status':'PASS','class':'P2_2_DERIVED_REAL_UL_B13_CONSUMER_PAYLOADS','host_count':48,'hemenv':sum('HemEnvLerp' not in r['name'] for r in recs),'hemenvlerp':sum('HemEnvLerp' in r['name'] for r in recs),'rdef_stripped':48,'b13_declared':48,'b13_refs_expected':[6,7,7],'p22_exact_base':48,'local_ul_island_only':48,'records':recs}
    (outroot/'data/UL48_LIVE_PAYLOAD_AUDIT.json').write_text(json.dumps(audit,indent=2)+'\n')
    print('PASS UL48',len(recs),'unique',len({r['ul_sha256'] for r in recs}))
if __name__=='__main__': main()
