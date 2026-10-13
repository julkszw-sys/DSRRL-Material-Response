#!/usr/bin/env python3
"""V264 second ordinary material gamma: exact RX33/34/35 pre-Fog consumer.

This does NOT replace the operator. It strictly proves two separate
nontrivial MAD reads downstream of original DSR EnvSpec/EnvDiffuse merge
and tests that the R19 source's mutation windows don't target them.
A completed R19 DXBC dump remains necessary for unconditional final
shader byte/pixel claims. The original owner retail DXBC isn't committed.
"""
import argparse,hashlib,json,struct
from pathlib import Path

RECEIVERS={
 33:{"sha":"7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85","gamma":11,"exp":1345,"early":1478,"late1":2112,"late2":2343,"envspec":1610,"merge":1802,"fog":2552,"debug":[1453,1465]},
 34:{"sha":"857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db","gamma":10,"exp":1254,"early":1387,"late1":2031,"late2":2262,"envspec":1519,"merge":1711,"fog":2471,"debug":[1362,1374]},
 35:{"sha":"9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8","gamma":9,"exp":914,"early":1047,"late1":1684,"late2":1915,"envspec":1179,"merge":1371,"fog":2124,"debug":[1022,1034]},
}
FOG_SHA="bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0"

def temporary(tok):
    return (tok&3) in(1,2) and ((tok>>12)&255)==0 and ((tok>>20)&3)==1

def dest_mask(tok):
    if (tok>>2)&3!=0:raise ValueError("not a destination mask")
    mask=(tok>>4)&15
    return {j for j in range(4) if (mask>>j)&1}

def source_swizzle(tok):
    mode=(tok>>2)&3
    if mode==1:return {(tok>>(4+2*j))&3 for j in range(4)}
    if mode==2:return {(tok>>4)&3}
    raise ValueError("unrecognized source swizzle")

def sections(blob):
    if blob[:4]!=b"DXBC" or len(blob)<36:
        raise ValueError("invalid DXBC signature")
    if struct.unpack_from("<I",blob,24)[0]!=len(blob):
        raise ValueError("DXBC container size mismatch")
    count=struct.unpack_from("<I",blob,28)[0]
    if not 1<=count<=64:raise ValueError("invalid DXBC directory")
    code=[]
    for i in range(count):
        off=struct.unpack_from("<I",blob,32+i*4)[0]
        if off+8>len(blob):raise ValueError("DXBC chunk index OOB")
        n=struct.unpack_from("<I",blob,off+4)[0]
        if off+8+n>len(blob):raise ValueError("DXBC chunk OOB")
        if blob[off:off+4] in (b"SHDR",b"SHEX"):
            if n%4:raise ValueError("unaligned shader")
            code.append(struct.unpack_from("<"+"I"*(n//4),blob,off+8))
    if len(code)!=1 or len(code[0])<3 or code[0][1]!=len(code[0]):
        raise ValueError("missing/ambiguous shader stream")
    words=list(code[0]);ins=[];i=2
    while i<len(words):
        count=(words[i]>>24)&127
        if count==0 or i+count>len(words):
            raise ValueError("bad DXBC opcode size")
        ins.append((i,words[i]&2047,words[i:i+count]))
        i+=count
    return words,ins

def references(inst,reg):
    _,_,w=inst;refs=[]
    for j in range(1,len(w)-1):
        if temporary(w[j]) and w[j+1]==reg:
            if j==1:refs.append(("WRITE",dest_mask(w[j])))
            else:refs.append(("READ",source_swizzle(w[j])))
    return refs

def unique_retained_window(w):
    if len(w)<214:return False
    matches=0
    for i in range(2,len(w)-211):
        digest=hashlib.sha256(struct.pack("<212I",*w[i:i+212])).hexdigest()
        if digest==FOG_SHA:matches+=1
    return matches==1

def audit(rx,blob):
    cfg=RECEIVERS[rx];sha=hashlib.sha256(blob).hexdigest()
    if sha!=cfg["sha"]:raise ValueError("wrong exact stock SHA RX"+str(rx))
    words,ins=sections(blob)
    pos={p:(p,op,v) for p,op,v in ins}
    if pos[cfg["exp"]][1]!=25 or pos[cfg["exp"]][2][2]!=cfg["gamma"]:
        raise ValueError("second material gamma producer changed")
    for key,opcode in (("early",16),("late1",50),("late2",50)):
        p=cfg[key]
        if pos[p][1]!=opcode or not any(t=="READ" and 0 in components
                      for t,components in references(pos[p],cfg["gamma"])):
            raise ValueError("material gamma RGB not consumed at "+str(p))
    if not cfg["envspec"]<cfg["merge"]<cfg["late1"]<cfg["late2"]<cfg["fog"]:
        raise ValueError("postmerge consumer outside the stock atmosphere cut")
    if not any(t=="WRITE" and {0,1,2}.issubset(c)
               for t,c in references(pos[cfg["late1"]],3)):
        raise ValueError("first postmerge MAD lacks r3.xyz write")
    if not any(t=="READ" and {0,1,2}.issubset(c)
               for t,c in references(pos[cfg["late2"]],3)):
        raise ValueError("second postmerge MAD lacks r3.xyz read")
    between=[p for p,op,w in ins if cfg["late1"]<p<cfg["late2"] and
             any(t=="WRITE" and c.intersection({0,1,2})
                 for t,c in references((p,op,w),3))]
    if between:raise ValueError("r3.xyz killed between postmerge MADs: "+str(between))
    debug_writes=[p for p,op,w in ins if cfg["exp"]<p<cfg["late2"] and
       any(t=="WRITE" and c.intersection({0,1,2})
           for t,c in references((p,op,w),cfg["gamma"]))]
    if debug_writes!=cfg["debug"]:
        raise ValueError("extra second-gamma producer/kill: "+str(debug_writes))
    # The two late MADs cannot be the R19 late SpecTex-alpha patch:
    # it matches MUL length7/dest .yzw, or MAD length9/dest .yzw.
    if len(pos[cfg["late1"]][2])!=10 or len(pos[cfg["late2"]][2])!=9 or pos[cfg["late2"]][2][1]!=0x00100072:
        raise ValueError("late MAD shape overlaps R19 material patch")
    # Exact DSR material Fresnel-like affine endpoint operation.
    # MAD r3.xyz, -r1.xxxx, rGamma.xyz, r1.xxxx
    # MAD rOut.xyz, r3.xyz, rK.component, rGamma.xyz
    # => rOut = M + (1-M) * (r1.x * rK.component)
    # The numeric interpretation of K as angular Fresnel is NOT proven.
    k_token,k_register,k_component={
       33:(0x00100556,7,"y"),
       34:(0x00100ff6,8,"w"),
       35:(0x00100ff6,7,"w"),
    }[rx]
    first_mad=pos[cfg["late1"]][2]
    next_mad=pos[cfg["late2"]][2]
    expected_first=[0x0a000032,0x00100072,3,
        0x80100006,0x41,1,
        0x00100246,cfg["gamma"],
        0x00100006,1]
    expected_second=[0x09000032,0x00100072,
        cfg["gamma"]+1,
        0x00100246,3,
        k_token,k_register,
        0x00100246,cfg["gamma"]]
    if first_mad!=expected_first or next_mad!=expected_second:
        raise ValueError("postmerge Fresnel-like affine operands differ from exact retail DSR")
    if not unique_retained_window(words):
        raise ValueError("native DSR Fog/LS subsequence not unique")
    return {
       "rx":rx,"original_shader_sha256":sha,
       "second_gamma_exp_word":cfg["exp"],"second_gamma_register":"r%d.xyz"%cfg["gamma"],
       "conditional_debug_override_words":cfg["debug"],"first_reader":cfg["early"],
       "post_envdiffuse_reader_MAD":cfg["late1"],"next_post_envdiffuse_MAD":cfg["late2"],
       "exact_DSR_affine_math":"M + (1-M)*K",
       "M":"pow(saturate(SpecTex.rgb*cb0[10].rgb),2.2)",
       "K":"r1.x * r%d.%s"%(k_register,k_component),
       "Fresnel_angular_semantic":"HYPOTHESIS; affine response proved, K source not fully proved",
       "intermediate_live_out":"r3.xyz","intervening_r3_xyz_writes":0,
       "exact_R19_rewrite_overlap_with_these_MADs":False,
       "stock_fog_start":cfg["fog"],"stock_fog_sha256":FOG_SHA,
       "level":"exact original donor opcodes + checked R19 nonoverlap source; materialized runtime shader not read",
       "R19_actual_materialized_PS_bytes":"UNOBSERVED",
       "owner_sidecar_draw":"UNOBSERVED","pixel_equivalence":"OPEN"
    }

def source_contract(root):
    material=(root/"renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf8")
    mr=(root/"renderer-core/src/operators/material_response/material_response_diffuse_v1.cpp").read_text(encoding="utf8")
    rgb=(root/"renderer-core/src/operators/resource_bridges/spec_rgb_consumer_materializer.cpp").read_text(encoding="utf8")
    exact=[
      "materialize_ptde_diffuse_response_v1(",
      "apply_exact_ptde_lerp_materialworkflow_r15_r16_r17(",
      "apply_exact_ptde_envdiffuse_lerp_consumer(",
      "materialize_spec_rgb_consumer(",
      "site->t12_word",
      "site->t11_word"]
    if not all(x in material for x in exact):
        raise ValueError("materializer source lineage changed")
    if "words[site] = 12u;" not in mr or "plan->diffuse_pow_site]" not in mr:
        raise ValueError("diffuse v1 source lineage changed")
    if not ('words.insert(' in rgb and 't1_sample->offset' in rgb and
            'c101_mul' in rgb and 'sample.size()' in rgb):
        raise ValueError("SpecRGB insertion-only location unclear")
    if "if (depth != 0u || in_else)" not in material:
        raise ValueError("R19 ordinary ELSE isolation changed")
    if "postmerge_gate_hits != 1u" not in material:
        raise ValueError("R19 postmerge-gate contract changed")
    return {"diffuse_v1_first_lane_only":True,
            "R19_envspec_cut_bounded_by_t12_to_t11":True,
            "R19_material_workflow_skips_ordinary_else":True,
            "R19_postmerge_gate_preimage_disjoint_from_late_MAD_pair":True,
            "SpecRGB_adds_instructions_after_original_t1_sample":True,
            "compiled_complete_R19_PS":"NOT_AVAILABLE_IN_SOURCE_ONLY_CI"}

def main():
    pa=argparse.ArgumentParser()
    pa.add_argument("--root",type=Path,default=Path("."))
    pa.add_argument("--donors",type=Path)
    pa.add_argument("--out",type=Path)
    args=pa.parse_args()
    doc={"schema":"dsrrl.v264.second_gamma_post_envdiffuse_liveness.v1",
         "r19_source_contract":source_contract(args.root),
         "real_original_donors":[],
         "activation":"NONE","pixel":"OPEN",
         "conclusion":"Second 2.2 material lane is not just an early compare; original DSR has a late gamma RGB MAD->r3.xyz->MAD between EnvDiffuse and Fog. R19 source does not target these exact instruction shapes. Runtime post-R19 bytecode and dynamic material provenance remain open."}
    if args.donors:
        doc["real_original_donors"]=[audit(rx,(args.donors/f"RX{rx}.dxbc").read_bytes()) for rx in RECEIVERS]
    if args.out:
        args.out.parent.mkdir(parents=True,exist_ok=True)
        args.out.write_text(json.dumps(doc,indent=2)+"\n",encoding="utf8")
    print("V264_POSTMERGE_SECOND_MATERIAL_GAMMA_SOURCE_AND_DONOR_CONSUMER_PASS",
          len(doc["real_original_donors"]))
if __name__=="__main__":main()
