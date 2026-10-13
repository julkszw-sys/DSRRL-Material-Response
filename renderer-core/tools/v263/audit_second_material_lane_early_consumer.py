#!/usr/bin/env python3
"""V263: exact original RX33/34/35 second material gamma early liveout.

Read-only owner original DXBC; no guessed per-draw branch selection or
claim that downstream R19 materializer preserves its pixel contribution.
"""
import argparse,hashlib,json,struct
from pathlib import Path
RX={
 33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",1345,11,1453,1465,1478,1610),
 34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",1254,10,1362,1374,1387,1519),
 35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",914,9,1022,1034,1047,1179)}
def unpack(b):
    if b[:4]!=b"DXBC" or struct.unpack_from("<I",b,24)[0]!=len(b):
        raise ValueError("bad DXBC")
    count=struct.unpack_from("<I",b,28)[0]
    res={}
    for k in range(count):
        p=struct.unpack_from("<I",b,32+4*k)[0]
        tag=b[p:p+4];n=struct.unpack_from("<I",b,p+4)[0]
        if p+8+n>len(b):raise ValueError("bad chunk")
        if tag in (b"SHEX",b"RDEF"):res[tag]=b[p+8:p+8+n]
    if b"SHEX" not in res or b"RDEF" not in res:raise ValueError("missing sections")
    w=struct.unpack("<"+"I"*(len(res[b"SHEX"])//4),res[b"SHEX"])
    ins=[];p=2
    while p<len(w):
        n=(w[p]>>24)&127
        if n==0 or p+n>len(w):raise ValueError("corrupt instruction")
        ins.append((p,w[p]&2047,w[p:p+n]))
        p+=n
    return res[b"RDEF"],ins
def debug_cb_index(rdef):
    n,off=struct.unpack_from("<II",rdef)
    for i in range(n):
        at=off+i*24
        nameoff,num,varoff,_,_,_=struct.unpack_from("<6I",rdef,at)
        if rdef[nameoff:rdef.index(b"\0",nameoff)]!=b"$Globals":continue
        for j in range(num):
            at2=varoff+j*40
            x,start,sz=struct.unpack_from("<III",rdef,at2)
            if rdef[x:rdef.index(b"\0",x)]==b"gFC_DebugDraw":
                if start!=1632 or sz!=16:raise ValueError("unexpected cb0[102] ABI")
                return 102
    raise ValueError("gFC_DebugDraw not found")
def temp(token):
    return (token&0x00f00000)==0x00100000 and (token>>12)&255==0
def trace(donors):
    out=[]
    for rx,(sha,exp,reg,zero,one,consumer,t12) in RX.items():
        b=(donors/f"RX{rx}.dxbc").read_bytes()
        if hashlib.sha256(b).hexdigest()!=sha:raise ValueError("receiver hash")
        rdef,ins=unpack(b)
        if debug_cb_index(rdef)!=102:raise ValueError("DebugDraw wrong")
        d={p:(op,w) for p,op,w in ins}
        for p,opcode in ((exp,25),(zero,54),(one,54),(consumer,16)):
            if p not in d or d[p][0]!=opcode:raise ValueError("instruction changed")
        if d[exp][1][1:3]!=(0x00100072,reg):
            raise ValueError("wrong second gamma liveout")
        writes=[];reads=[]
        for p,op,w in ins:
            if p<=exp:continue
            for j in range(1,len(w)-1):
                if temp(w[j]) and w[j+1]==reg:
                    (writes if j==1 else reads).append(p)
        if writes[:2]!=[zero,one] or reads[0]!=consumer or consumer>=t12:
            raise ValueError("second material early consumer mismatch")
        out.append({"rx":rx,"sha256":sha,"second_gamma_result":"r%d.xyz"%reg,
          "source_exp_at":exp,"debug_draw_cb":102,
          "conditional_debug_overwrites":[zero,one],
          "first_early_read_after_debug_switch":consumer,
          "r19_envspec_replacement_cut_begins":t12,
          "confirmed":"retail stock producer+early read only",
          "post_R19_dynamic_liveness":"OPEN","PTDE_pixel":"OPEN"})
    return out
if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--donors",required=True,type=Path)
    p.add_argument("--out",type=Path)
    a=p.parse_args()
    doc={"schema":"dsrrl.v263.second_material_lane_early_consumer.v1",
         "scope":"retail stock DXBC, not post-R19 assembled image",
         "rows":trace(a.donors)}
    if a.out:a.out.write_text(json.dumps(doc,indent=2)+"\n",encoding="utf8")
    print("V263_STOCK_RX33_35_SECOND_MATERIAL_LANE_EARLY_CONSUMER_PASS")
