#!/usr/bin/env python3
"""V260: prove material CB0[85] branch and ELSE remains DSR under R19.

Reads owner-provided original RX33/RX34/RX35 stock DXBC. No extracted
game bytecode is committed, no Fog/LightScattering patch, no live hook.
Does NOT equate authored MTD default with effective GPU cb0[85].
"""
from pathlib import Path
import argparse,hashlib,json,struct
RX={
 33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",1081,1292,1394),
 34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",990,1201,1303),
 35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",650,861,963),
}
IF_CB85=[0x0400001f,0x0020800a,0,85]
def globals_rdef(rdef):
    # DX11 RDEF: constant buffers, then each $Globals variable descriptor.
    count,off=struct.unpack_from("<II",rdef,0)
    records={}
    for i in range(count):
        p=off+i*24
        name_pos,nvars,var_pos,size,_,_=struct.unpack_from("<6I",rdef,p)
        name=rdef[name_pos:rdef.index(bytes((0,)),name_pos)].decode("ascii")
        if name != "$Globals":continue
        for j in range(nvars):
            v=var_pos+j*40
            name_pos,byte_offset,nbytes,_,_,_=struct.unpack_from("<6I",rdef,v)
            key=rdef[name_pos:rdef.index(bytes((0,)),name_pos)].decode("ascii")
            records[key]={"byte_offset":byte_offset,"register":byte_offset//16,"bytes":nbytes}
    for key,reg in (("gFC_MaterialWorkflow",85),
                    ("gFC_DifMapMulCol",9),
                    ("gFC_MaterialOverrideParams",100)):
        if key not in records or records[key]["byte_offset"]!=16*reg:
            raise ValueError("V261 RDEF parameter ABI mismatch: "+key)
    return records

def read_dxbc(path):
    blob=path.read_bytes()
    if blob[:4]!=b"DXBC" or len(blob)<40:raise ValueError("invalid DXBC")
    n=struct.unpack_from("<I",blob,28)[0]
    if not 1<=n<=64:raise ValueError("invalid chunk count")
    out=[];rdef=None
    for i in range(n):
        o=struct.unpack_from("<I",blob,32+4*i)[0]
        if o+8>len(blob):raise ValueError("bad chunk")
        tag=blob[o:o+4];size=struct.unpack_from("<I",blob,o+4)[0]
        if o+8+size>len(blob):raise ValueError("bad chunk span")
        if tag==b"RDEF":rdef=blob[o+8:o+8+size]
        if tag in (b"SHEX",b"SHDR"):
            if size%4:raise ValueError("unaligned shader")
            out.append(list(struct.unpack_from("<"+"I"*(size//4),blob,o+8)))
    if len(out)!=1 or out[0][1]!=len(out[0]):raise ValueError("shader missing")
    w=out[0]; ins=[];i=2
    while i<len(w):
        size=(w[i]>>24)&127
        if size<=0 or i+size>len(w):raise ValueError("invalid ISA")
        ins.append((i,w[i]&2047,w[i:i+size]))
        i+=size
    if rdef is None:raise ValueError("V261 missing RDEF")
    return blob,w,ins,globals_rdef(rdef)
def branch(rx,donors):
    b,w,ins,abi=read_dxbc(donors/f"RX{rx}.dxbc")
    sha,if_off,else_off,end_off=RX[rx]
    if hashlib.sha256(b).hexdigest()!=sha:raise ValueError("donor SHA mismatch")
    ix=[i for i,(_,op,t) in enumerate(ins) if op==31 and t==IF_CB85]
    if len(ix)!=1:raise ValueError("cb0[85] branch not unique")
    k=ix[0];depth=0;else_ix=None;end_ix=None
    for j in range(k+1,len(ins)):
        op=ins[j][1]
        if op==31:depth+=1
        elif op==21:
            if depth==0:end_ix=j;break
            depth-=1
        elif op==18 and depth==0:
            if else_ix is not None:raise ValueError("nonunique ELSE")
            else_ix=j
    if else_ix is None or end_ix is None:raise ValueError("unbalanced branch")
    if (ins[k][0],ins[else_ix][0],ins[end_ix][0]) !=(if_off,else_off,end_off):
        raise ValueError("unexpected branch offsets")
    def arm(a,c):
        region=ins[a:c];start=region[0][0];stop=ins[c][0]
        raw=struct.pack("<"+"I"*(stop-start),*w[start:stop])
        refs=[]
        for _,_,t in region:
            for i in range(len(t)-2):
                if (t[i]>>12)&255==8 and t[i+1]==0:
                    refs.append(t[i+2])
        return dict(instructions=len(region),words=stop-start,
            cb0_refs={str(i):refs.count(i) for i in sorted(set(refs))},
            sha256=hashlib.sha256(raw).hexdigest())
    ordinary_else=ins[else_ix+1:end_ix]
    gamma_chains=[]
    for j in range(len(ordinary_else)-2):
        a,b2,c=ordinary_else[j:j+3]
        if (a[1],b2[1],c[1]) != (47,56,25):continue
        if len(b2[2])==10 and b2[2][-4:-1] == [0x400ccccd]*3:
            gamma_chains.append({"log":a[0],"mul_2_2":b2[0],"exp":c[0]})
    expected={
        33:((1301,1307,1317),(1330,1335,1345)),
        34:((1210,1216,1226),(1239,1244,1254)),
        35:((870,876,886),(899,904,914))}
    if tuple(tuple(v.values()) for v in gamma_chains)!=expected[rx]:
        raise ValueError("V261 ordinary ELSE gamma law differs from original")
    return {"receiver":rx,"donor_sha256":sha,"RDEF_gFC_MaterialWorkflow":abi["gFC_MaterialWorkflow"],"ordinary_ELSE_gamma2_2":gamma_chains,"if_word":if_off,
            "else_word":else_off,"endif_word":end_off,
            "if_arm":arm(k+1,else_ix),"else_arm":arm(else_ix+1,end_ix)}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--donors",required=True,type=Path)
    p.add_argument("--out",required=True,type=Path)
    p.add_argument("--exe",type=Path,default=None)
    a=p.parse_args()
    rows=[branch(i,a.donors) for i in RX]
    report={"schema":"dsrrl.v260.hemenvlerp_workflow_branch.v1",
      "scope":"RX33-35 original DSR DXBC; no stock Fog modification",
      "rx":rows,"r19_code_scope":"MaterialWorkflow R15/16/17 changes only IF arm, not ELSE",
      "cb0_85_RDEF":"gFC_MaterialWorkflow, 16 bytes at byte offset 1360",
      "DSR_ordinary_ELSE":"two log-mul(2.2)-exp chains for material products",
      "PTDE_reference":"DifSpcBmp HemEnvLerp ps_3_0 0663..0679 direct c100 diffuse material multiplication",
      "cb0_85_draw_specific_value":"UNOBSERVED",
      "nonmetal_overgloss_cause":"HYPOTHESIS",
      "bridge_activation":"NOT_IMPLEMENTED","pixel":"OPEN"}
    if a.exe is not None:
        exe=a.exe.read_bytes()
        string_at=exe.find("g_MaterialWorkflow".encode("utf-16-le"))
        if string_at<0:raise ValueError("DSR EXE does not contain g_MaterialWorkflow")
        # Exact original exe image map: original writable source not modified.
        text_va=0x140293538;text_off=0x400+(text_va-0x140001000)
        if exe[text_off:text_off+3]!=bytes.fromhex("4c8d0d"):
            raise ValueError("unexpected DSR material producer xref")
        disp=struct.unpack_from("<i",exe,text_off+3)[0]
        string_va=0x14129d000+(string_at-0x129b800)
        if text_va+7+disp!=string_va:
            raise ValueError("material name producer xref mismatch")
        zero_init=0x400+(0x140292d7f-0x140001000)
        if exe[zero_init:zero_init+3]!=bytes.fromhex("4533f6"):
            raise ValueError("V261 r14 default ZERO not proven")
        forced_zero=0x400+(0x1402935b8-0x140001000)
        if exe[forced_zero:forced_zero+10]!=bytes.fromhex("f786e401000000010000"):
            raise ValueError("V261 MaterialWorkflow feature bit preimage differs")
        if exe[forced_zero+10:forced_zero+15]!=bytes.fromhex("7503448930"):
            raise ValueError("V261 forced-zero feature branch differs")
        report["mtl_producer"]={"field_name":"g_MaterialWorkflow",
          "utf16_file_offset":string_at,"lookup_xref_va":hex(text_va),
          "typed_MTD_destination_offset":"0x220",
          "absence_default":"0 from xor r14d,r14d at 0x140292d7f",
          "feature_bit0x100_unset":"forces zero at material+0x220 (0x1402935b8)",
          "producer_to_cb0_85":"OPEN"}
    a.out.parent.mkdir(parents=True,exist_ok=True)
    a.out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print("V260_ORIGINAL_RX33_RX34_RX35_MATERIAL_IF_ELSE_AUDIT_PASS")
if __name__=="__main__":main()
