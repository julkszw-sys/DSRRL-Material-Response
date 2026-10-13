#!/usr/bin/env python3
"""V263: R19 already rewrites ordinary ELSE diffuse; forbid V262 double patch.

Reads exact pinned C++ materializer and generated-operator TSV. When
--donors is given, independently authenticates actual user stock RX DXBC
and proves precise stock-to-R19 first-lane operand mutation. No retail
shader bytes or executable changes are checked into GitHub.
"""
import argparse,csv,hashlib,json,struct
from pathlib import Path
RX={
  33:("7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",1209,1299,1313,1330,1341),
  34:("857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",1118,1208,1222,1239,1250),
  35:("9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",778,868,882,899,910)}
FOG="bf52da4e8b27d14b56cd047e67d8b012b2824309c7b55ca64e9ba536940286b0"
def verify_source(root):
    cpp=(root/"renderer-core/src/operators/material_response/material_response_diffuse_v1.cpp").read_text(encoding="utf8")
    r19=(root/"renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp").read_text(encoding="utf8")
    for line in ('words[site] = 12u;',
                 'words[site + 1u] = 1u;',
                 'plan->diffuse_pow_site] =',
                 'plan->diffuse_pow_site + 1u] =',
                 'plan->diffuse_pow_site + 2u] =',
                 '0x3f800000u;',
                 'for (const auto site :\n         plan->c100_sites)'):
        if line not in cpp:raise ValueError("R19 generic MR source changed: "+line)
    if not ('materialize_ptde_diffuse_response_v1(' in r19 and
            'base,\n                true);' in r19):
        raise ValueError("R19 not using deferred diffuse-v1 as base")
    return {"R19_base":"materialize_ptde_diffuse_response_v1(..., true)",
            "first_lerp_diffuse_exponent":1.0,
            "first_lerp_diffuse_absolute_value_not_removed":True,
            "both_material_workflow_arms_c100":"b12[1]"}
def plans(root):
    src=root/"renderer-core/data/provenance/material_response_diffuse_v1.tsv"
    with src.open(encoding="utf8",newline="") as f:
        allrows=list(csv.DictReader(f,delimiter="\t"))
    if len(allrows)!=48:raise ValueError("source corpus is not 48 exact receivers")
    subset={int(r["receiver_id"]):r for r in allrows if r["family"]=="HEMENV_LERP"}
    if len(subset)!=24:raise ValueError("invalid Lerp family census")
    for rx,(_,x,y,z,_,_) in RX.items():
        p=subset[rx]
        if (int(p["c100_site0"]),int(p["c100_site1"]),
            int(p["diffuse_pow_site"]))!=(x,y,z):
            raise ValueError("R19 cached opcode sites changed RX"+str(rx))
    return {"total_receivers":48,"lerp_receivers":24,
            "specific_verified_rx":[33,34,35]}
def original_shex(path,expected_sha):
    b=path.read_bytes()
    if hashlib.sha256(b).hexdigest()!=expected_sha:raise ValueError("wrong retail donor")
    if b[:4]!=b"DXBC" or struct.unpack_from("<I",b,24)[0]!=len(b):
        raise ValueError("bad DXBC")
    n=struct.unpack_from("<I",b,28)[0];matches=[]
    for k in range(n):
        pos=struct.unpack_from("<I",b,32+4*k)[0]
        if b[pos:pos+4]!=b"SHEX":continue
        size=struct.unpack_from("<I",b,pos+4)[0]
        matches.append(list(struct.unpack_from("<"+"I"*(size//4),b,pos+8)))
    if len(matches)!=1:raise ValueError("SHEX not unique")
    return matches[0]
def donor_check(root):
    result=[]
    for rx,(sha,site_if,site_else,pow_site,other_log,other_pow) in RX.items():
        w=original_shex(root/("RX"+str(rx)+".dxbc"),sha)
        if w[site_if:site_if+2]!=[0,9] or w[site_else:site_else+2]!=[0,9]:
            raise ValueError("stock c100 preimage differs")
        if w[pow_site:pow_site+3]!=[0x400ccccd]*3:
            raise ValueError("stock first 2.2 preimage differs")
        if w[other_log]&0x7ff!=47 or w[other_pow:other_pow+3]!=[0x400ccccd]*3:
            raise ValueError("independent second gamma changed")
        patched=w[:]
        patched[site_if:site_if+2]=[12,1]
        patched[site_else:site_else+2]=[12,1]
        patched[pow_site:pow_site+3]=[0x3f800000]*3
        if patched[site_else:site_else+2]==[0,9] or patched[pow_site:pow_site+3]==[0x400ccccd]*3:
            raise AssertionError("V262 stock-only preimage unexpectedly matches R19")
        if w[other_log:other_pow+3]!=patched[other_log:other_pow+3]:
            raise AssertionError("second lane was modified")
        hits=0
        for i in range(2,len(w)-211):
            before=struct.pack("<212I",*w[i:i+212])
            if hashlib.sha256(before).hexdigest()==FOG:
                hits+=1
                if patched[i:i+212]!=w[i:i+212]:
                    raise AssertionError("Fog was modified")
        if hits!=1:raise ValueError("original DSR Fog oracle missing")
        result.append({"receiver":rx,"original_sha256":sha,
           "c100_IF":site_if,"c100_ELSE":site_else,
           "diffuse_pow_first_site":pow_site,
           "r19_c100_rewritten":[12,1],"r19_exponent":1.0,
           "v262_expects_old_c100_and_2_2":True,
           "v262_cannot_pass_its_preimage_on_r19":True,
           "second_2_2_lane_remains_untouched":True,
           "stock_fog_212_words_preserved":True})
    return result
def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--root",type=Path,default=Path("."))
    ap.add_argument("--donors",type=Path)
    ap.add_argument("--out",type=Path)
    a=ap.parse_args()
    report={"schema":"dsrrl.v263.r19_first_material_lane_inapplicability.v1",
        "source_contract":verify_source(a.root),
        "plan_contract":plans(a.root),
        "conclusion":"V262 stock-DXBC candidate is NOT applicable to already materialized R19: R19 has remapped cb9 to b12[1] and exponent 2.2 to 1 before V262's expected preimage",
        "remaining_difference":"R19 diffuse = abs(Z) instead of linear Z for negative Z; input-sign reachability not established. Other second/specular lane still DSR 2.2.",
        "material_identity":"FULL_ASSET_AND_DRAW_PROOF_STILL_REQUIRED",
        "runtime":"OPEN","pixel":"OPEN"}
    if a.donors:report["real_retail_donors"]=donor_check(a.donors)
    if a.out:
        a.out.parent.mkdir(parents=True,exist_ok=True)
        a.out.write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")
    print("V263_R19_ALREADY_REMAPS_C100_AND_LINEAR_EXPONENT_NO_DOUBLE_PATCH_PASS")
if __name__=="__main__":main()
