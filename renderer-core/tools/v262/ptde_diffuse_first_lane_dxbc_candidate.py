#!/usr/bin/env python3
"""V262: exact RX33/34/35 ordinary-ELSE PTDE diffuse-factor subcut candidate.

NO runtime activation or generalized receiver-wide patch. The output is
a *candidate* (never a release shader): physical FLVER/MTD/slot/sidecar
authority, complete downstream material/specular operator mapping and live
CB0[85] still need verification.

Stock-oracle mode proves the assembly replacement and checksum using
the owner's exact ORIGINAL DSR donors; R19-candidate mode additionally
requires declared b12[0..5], substituting only cb0[9] -> b12[1] (PTDE c100).
The separate second MaterialWorkflow/Specular gamma lane stays stock.
"""
import argparse,hashlib,json,math,struct
from pathlib import Path
RECEIVERS = {
  33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",1292,1394,9),
  34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",1201,1303,8),
  35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",861,963,7),
}
FOG_SHA="bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0"
K=[int(abs(math.sin(i+1))*2**32) for i in range(64)]
S=[7,12,17,22]*4+[5,9,14,20]*4+[4,11,16,23]*4+[6,10,15,21]*4
MASK=0xffffffff

def transform(st,block):
    words=struct.unpack("<16I",block)
    a,b,c,d=st;aa,bb,cc,dd=st
    for i in range(64):
        if i<16:f=(b&c)|((~b)&d);g=i
        elif i<32:f=(d&b)|((~d)&c);g=(5*i+1)%16
        elif i<48:f=b^c^d;g=(3*i+5)%16
        else:f=c^(b|(~d));g=7*i%16
        old_d=d;d=c;c=b
        v=(a+f+K[i]+words[g])&MASK
        b=(b+((v<<S[i])|(v>>(32-S[i]))))&MASK
        a=old_d
    return [(aa+a)&MASK,(bb+b)&MASK,(cc+c)&MASK,(dd+d)&MASK]

def dxbc_digest(blob):
    payload=blob[20:]
    bit_count=len(payload)*8
    st=[0x67452301,0xefcdab89,0x98badcfe,0x10325476]
    full=len(payload)&~63
    for i in range(0,full,64):st=transform(st,payload[i:i+64])
    tail=payload[full:]
    block=bytearray(64)
    if len(tail)>=56:
        block[:len(tail)]=tail;block[len(tail)]=0x80
        st=transform(st,block);block=bytearray(64)
        struct.pack_into("<I",block,0,bit_count&MASK)
    else:
        struct.pack_into("<I",block,0,bit_count&MASK)
        block[4:4+len(tail)]=tail
        block[4+len(tail)]=0x80
    struct.pack_into("<I",block,60,((bit_count>>2)|1)&MASK)
    st=transform(st,block)
    return struct.pack("<4I",*st)

def words_and_offset(blob):
    if blob[:4]!=b"DXBC" or len(blob)<40:
        raise ValueError("not a DXBC container")
    if struct.unpack_from("<I",blob,24)[0]!=len(blob):
        raise ValueError("incorrect DXBC size")
    if dxbc_digest(blob)!=blob[4:20]:
        raise ValueError("invalid DXBC checksum")
    n=struct.unpack_from("<I",blob,28)[0]
    if not 1<=n<=64:raise ValueError("invalid chunk count")
    code=[]
    for i in range(n):
        off=struct.unpack_from("<I",blob,32+4*i)[0]
        if off+8>len(blob):raise ValueError("out-of-range DXBC offset")
        size=struct.unpack_from("<I",blob,off+4)[0]
        if off+8+size>len(blob):raise ValueError("out-of-range DXBC chunk")
        if blob[off:off+4] in (b"SHEX",b"SHDR"):
            if size%4:raise ValueError("unaligned DXBC code")
            code.append((off+8,struct.unpack_from("<"+"I"*(size//4),blob,off+8)))
    if len(code)!=1:raise ValueError("ambiguous or absent SHEX")
    start,raw=code[0]
    if len(raw)<2 or raw[1]!=len(raw):
        raise ValueError("wrong SHEX DWORD count")
    return start,list(raw)

def instructions(w):
    ans=[];p=2
    while p<len(w):
        size=(w[p]>>24)&127
        if not size or p+size>len(w):
            raise ValueError("bad SHEX instruction length")
        ans.append((p,w[p]&2047,w[p:p+size]))
        p+=size
    return ans

def stock_fog_window(words):
    # SHA-certified original stock DSR Fog+LS region, no relative-offset
    # assumptions about generated R19 insertions.
    fmt="<"+"I"*212
    hits=[]
    for p in range(2,len(words)-211):
        if hashlib.sha256(struct.pack(fmt,*words[p:p+212])).hexdigest()==FOG_SHA:
            hits.append(p)
    if len(hits)!=1:
        raise ValueError("absent / duplicated stock Fog and LS 212-word span")
    return hits[0],hits[0]+212

def else_first_lane(w,rx):
    ins=instructions(w)
    ifs=[j for j,(p,op,v) in enumerate(ins)
         if op==31 and v==[0x0400001f,0x0020800a,0,85]]
    if len(ifs)!=1:raise ValueError("nonunique cb0[85].x material IF")
    depth=0;el=None;end=None
    for j in range(ifs[0]+1,len(ins)):
        op=ins[j][1]
        if op==31:depth+=1
        elif op==21:
            if depth==0:end=j;break
            depth-=1
        elif op==18 and depth==0:
            if el is not None:raise ValueError("two ordinary ELSE arms")
            el=j
    if el is None or end is None:
        raise ValueError("unbalanced gFC_MaterialWorkflow branch")
    # ELSE first lane has MUL(cb0[9]), LOG(abs), MUL(2.2), EXP(dst).
    a=ins[el+1:el+5]
    if len(a)!=4 or [(o,len(v)) for _,o,v in a]!=[
        (56,8),(47,6),(56,10),(25,5)]:
        raise ValueError("unexpected first diffuse-factor operator")
    (mp,_,mul),(lp,_,log),(gp,_,gamma),(ep,_,exp)=a
    if mul[-3:]!=[0x00208246,0,9] or mul[1:5]!=[
        0x00100072,1,0x00100246,1]:
        raise ValueError("unexpected diffuse cb0[9] multiply")
    if log[1:]!=[0x00100072,1,0x80100246,0x81,1]:
        raise ValueError("unexpected abs/log diffuse input")
    if gamma[1:]!=[0x00100072,1,0x00100246,1,0x00004002,
                   0x400ccccd,0x400ccccd,0x400ccccd,0]:
        raise ValueError("unexpected two-point-two diffuse gamma")
    if exp!=[0x05000019,0x00100072,RECEIVERS[rx][3],
             0x00100246,1]:
        raise ValueError("unexpected diffuse gamma output register")
    # The other arm is an independent, saturated/specular gamma term.
    second=ins[el+5:el+8]
    if [o for _,o,_ in second]!=[56,47,56]:
        raise ValueError("second material lane not preserved")
    if second[0][2][0]!=0x08002038 or second[0][2][-3:]!=[
        0x00208246,0,10]:
        raise ValueError("missing stock second lane cb0[10] and SAT")
    if second[2][2][-5:-1]!=[
        0x00004002,0x400ccccd,0x400ccccd,0x400ccccd]:
        raise ValueError("second lane 2.2 preimage mismatch")
    return {"mul":mp,"log":lp,"gamma":gp,"exp":ep,
            "second_start":second[0][0],"second_end":ins[end][0],
            "if":ins[ifs[0]][0],"else":ins[el][0]}

def patch(blob,rx,r19=False):
    start,w=words_and_offset(blob)
    fog=stock_fog_window(w)
    sites=else_first_lane(w,rx)
    original=list(w)
    if r19:
        # R19 b12[0..5] must already be declared and initialized as an
        # EXACT PTDE material carrier by the integrated addon runtime.
        if not any(w[i:i+4]==[0x04000059,0x00208e46,12,6]
                   for i in range(2,len(w)-3)):
            raise ValueError("R19 b12[0..5] carrier is missing")
        p=sites["mul"]
        # The exact PTDE c100 material coefficient is b12[1].xyz.
        w[p+6],w[p+7]=12,1
    # Remove only first lane ABS/LOG and *2.2 via DXBC 1-DWORD NOPs,
    # then write its original *linear* RGB to the same live-out register.
    start_log=sites["log"]
    end_gamma=sites["exp"]
    assert end_gamma-start_log==16
    w[start_log:end_gamma]=[0x0100003a]*16
    out=RECEIVERS[rx][3]
    w[end_gamma:end_gamma+5]=[
        0x05000036,0x00100072,out,0x00100246,1]
    diff={i:(a,b) for i,(a,b) in enumerate(zip(original,w)) if a!=b}
    allowed=set(range(start_log,end_gamma+5))
    if r19:allowed.update((sites["mul"]+6,sites["mul"]+7))
    if not set(diff).issubset(allowed):raise AssertionError("changed outside diffuse lane")
    if original[sites["second_start"]:sites["second_end"]]!=w[
            sites["second_start"]:sites["second_end"]]:
        raise AssertionError("stock second/specular material lane changed")
    if original[fog[0]:fog[1]]!=w[fog[0]:fog[1]]:
        raise AssertionError("stock DSR Fog/LightScattering changed")
    assert len(instructions(w))>0 and stock_fog_window(w)==fog
    result=bytearray(blob)
    struct.pack_into("<"+"I"*len(w),result,start,*w)
    result[4:20]=dxbc_digest(result)
    if dxbc_digest(result)!=result[4:20]:
        raise AssertionError("candidate checksum invalid")
    _,check=words_and_offset(result)
    if check!=w:raise AssertionError("candidate did not survive DXBC parse")
    return bytes(result),{"rx":rx,"mode":"R19_PTDE_C100" if r19 else "STOCK_GAMMA_ORACLE",
        "input_sha256":hashlib.sha256(blob).hexdigest(),
        "candidate_sha256":hashlib.sha256(result).hexdigest(),
        "stock_fog_ls_region":FOG_SHA,"fog_words_unchanged":True,
        "stock_specular_material_lane_unchanged":True,
        "changed_word_count":len(diff),"gamma_opcodes_replaced_by_nops":16,
        "ptde_c100_b12_1_applied":r19,
        "live_material_slot_eligibility":"NOT_AUTHORIZED",
        "live_renderer_activation":"NOT_IMPLEMENTED",
        "pixel_equivalence":"OPEN"}

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--stock-dir",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    ap.add_argument("--r19-materialized",type=Path)
    ap.add_argument("--receiver",type=int,choices=(33,34,35))
    args=ap.parse_args()
    records=[]
    for rx,(sha,_,_,_) in RECEIVERS.items():
        original=(args.stock_dir/f"RX{rx}.dxbc").read_bytes()
        if hashlib.sha256(original).hexdigest()!=sha:
            raise ValueError("wrong exact original receiver: RX"+str(rx))
        candidate,entry=patch(original,rx,r19=False)
        if candidate==original:raise AssertionError("no material operator change")
        # A mutated stock material donor cannot pass the exact SHA gate.
        bad=bytearray(original);bad[123]^=1
        if hashlib.sha256(bad).hexdigest()==sha:
            raise AssertionError("donor identity check failed")
        records.append(entry)
    if args.r19_materialized:
        if args.receiver is None:
            raise ValueError("R19 materialized candidate needs exact --receiver")
        original=args.r19_materialized.read_bytes()
        # This is only an offline shader candidate. It is NOT eligible
        # for use in live game without per-draw sidecar and slot proof.
        _,entry=patch(original,args.receiver,r19=True)
        records.append(entry)
    output={"schema":"dsrrl.v262.ptde_diffuse_first_lane.v1",
        "source":"RX33/34/35 original retail DSR, NO redistribution of game DXBC",
        "scope":"R19 ordinary ELSE first diffuse lane ONLY, before stock Fog",
        "material_gating":"UNPROVEN: cannot replace shared shader globally",
        "status":"OFFLINE_CODE_PATCH_ONLY","records":records}
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(output,indent=2)+"\n",encoding="utf8")
    print("V262_THREE_REAL_RX_FIRST_DIFFUSE_LANE_OPCODE_AND_CHECKSUM_PASS")
if __name__=="__main__":main()
