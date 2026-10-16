#!/usr/bin/env python3
"""V313 original PTDE Metal slot2 / Wet slot3 exact shader operator islands.

One build-time source-of-truth compiler: reject missing/ambiguous MTDs,
verify original DSR raw SHA, PTDE/DSR identical DifSpcBmp SPX, exact
authored float bits and EnvSpec transport slot. Compile after V312.
Never borrow P_Metal route345 if already active; stock DSR on misses.
"""
import json,re,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
db=json.loads((root/"data/census/ptde_mtd_envspec_router_v1.json").read_text("utf-8"))
assert db["source_binary"]["record_count"]==325
groups={
 2:[
 "P_Metal[DSB]_Edge.mtd","P_Metal[DSB]_Alp.mtd",
 "C_Metal[DSB].mtd","C_Metal[DSB]_Alp.mtd","C_Metal[DSB]_Edge.mtd",
 "C_2320_Metal[DSB].mtd",
 "M_7Metal[DSB].mtd","M_7Metal[DSB]_Alp.mtd",
 "M_7Metal[DSB]_DetB.mtd","M_7Metal[DSB]_Edge.mtd",
 "Ps_Hair[DSB].mtd","Ps_Hair[DSB]_Alp.mtd","Ps_Hair[DSB]_Edge.mtd",
 "A05_7Metal[DSB].mtd"
 ],
 3:[
 "P_Wet[DSB].mtd","P_Wet[DSB]_Edge.mtd",
 "C_Wet[DSB].mtd","C_Wet[DSB]_Alp.mtd","C_Wet[DSB]_Edge.mtd",
 "C_3530_Unique_Wet[DSB].mtd",
 "C_3530_Unique_Wet[DSB]_Edge.mtd",
 "C_5290_Body[DSB].mtd"
 ]
}
# Metal & Wet are independent source-to-draw families with original
# identical SPX; no P_Metal base duplicate (existing route345 untouched).
assert len(groups[2])==14 and len(groups[3])==8
def fv(bits):
    assert re.fullmatch(r"0x[0-9a-f]{8}",bits)
    v=struct.unpack(">f",struct.pack(">I",int(bits,16)))[0]
    assert v==v and 0<=v<=128
    return v
def cpp(v):
    s=format(v,".9g")
    if "." not in s and "e" not in s and "E" not in s:s+=".0"
    assert struct.unpack(">f",struct.pack(">f",float(s)))[0]==v
    return s+"f"
def xrow(name,slot):
    hit=[x for x in db["records"] if x["mtd_name"]==name]
    assert len(hit)==1,(name,"ambiguous DSR raw-MTD source")
    x=hit[0]
    assert x["envspc_slot"]==slot
    assert x["state"]=="PRESENT" and x["homology_class"]=="HOMOLOGOUS_SPC"
    assert x["delta_kind"]=="EXACT_SPX"
    assert x["ptde_spx"]==x["dsr_spx"]
    assert x["ptde_spx"].endswith("FRPG_Phn_ColDifSpcBmp.spx")
    assert len(x["c100_f32_bits"])==3 and len(set(x["c100_f32_bits"]))==1
    assert len(x["c101_f32_bits"])==3 and len(set(x["c101_f32_bits"]))==1
    a,b,c=(fv(x["c100_f32_bits"][0]),fv(x["c101_f32_bits"][0]),fv(x["c102_f32_bits"]))
    assert c>0
    sha=x["dsr_mtd_sha256"]
    assert re.fullmatch(r"[0-9a-f]{64}",sha)
    # Raw MTD bytes can be shared by multiple semantic MTD names in the
    # original database (P_Metal_Edge and S_Metal_Edge). Do not equate
    # those semantic identities! This route is authorized only after the
    # independent exact FLVER SHA+slot->MTD basename proof and raw SHA.
    # Reject any duplicate (name, SHA) source record, not other names
    # sharing bytewise-identical original material payloads.
    assert sum(y["mtd_name"]==name and
               y["dsr_mtd_sha256"]==sha for y in db["records"])==1
    return x,a,b,c
rows=[];report=[]
for slot in (2,3):
    for name in groups[slot]:
        x,a,b,c=xrow(name,slot)
        route=0x56000000+int(x["index"])
        rows.append('{"'+name+'","'+x["dsr_mtd_sha256"]+'",'+
                    f"{slot}u,0x{route:08x}u,{cpp(a)},{cpp(b)},{cpp(c)}"+'}')
        report.append(dict(name=name,slot=slot,route=route,index=x["index"],
            sha=x["dsr_mtd_sha256"],c100=x["c100_f32_bits"],
            c101=x["c101_f32_bits"],c102=x["c102_f32_bits"]))
assert len(set(x["sha"] for x in report))==22
header=r"""#pragma once
// V313 original PTDE Metal slot2 and Wet slot3 operator islands.
// Require independent exact live FLVER SHA+slot+MTD, original raw MTD SHA,
// source MTD exact homogeneous DifSpcBmp SPX and RX33..35 consumer.
// All resources/PS/CB/EnvSpec probe/b12 state are gated downstream.
// Inactive host only, never suppress R43 P_Metal route345 or unrelated MR.
#include "dsrrl/runtime/v300_exact_leather_material.hpp"
#include <cstdint>
#include <cstddef>
namespace dsrrl::runtime::v313 {
namespace mr=dsrrl::operators::material_response;
struct row {
    const char *name; const char *sha;
    std::uint32_t env_slot; std::uint32_t route;
    float c100,c101,power;
};
inline constexpr row k_rows[]={
"""+",\n".join(rows)+r"""
};
inline constexpr std::size_t k_count=sizeof(k_rows)/sizeof(k_rows[0]);
static_assert(k_count==22u,"V313 14 exact metal + 8 wet original sources");

inline bool promote(const mr::material_identity&m,const mr::decision&host,
                    unsigned rx,mr::decision&out) noexcept {
    out=host;
    if(host.active||!m.valid||!m.actual_material_exact||
       !m.owner_tuple_exact||!m.material_slot_valid||
       rx<33u||rx>35u||
       m.material_family_hash!=mr::mtd_semantic_hash("DifSpcBmp"))
        return false;
    const row*hit=nullptr;
    for(const auto &r:k_rows) {
        if(m.semantic_name_hash!=mr::mtd_semantic_hash(r.name) ||
           !v297::raw_sha_equal(m.raw_mtd_sha256,r.sha))
            continue;
        if(hit) return false;
        hit=&r;
    }
    if(!hit)return false;
    auto q=dsrrl::runtime::v203_equipment::make_envspec36_query(m);
    if(!mr::has_exact_flver_material_ownership(q))return false;
    auto e=mr::classify_mtd_envspec_semantics(q);
    if(!e.exact_identity_match ||
       e.presence!=mr::ptde_envspec_presence::present ||
       e.router_state!=mr::mtd_envspec_router_state::present ||
       !e.envspc_slot_valid||e.envspc_slot!=hit->env_slot)
       return false;
    out.active=true;
    out.reason=mr::decision_reason::active;
    out.receiver_id=rx;
    out.route_index=hit->route;
    out.certified_operations=mr::diffuse_material_domain_linear |
                             mr::specular_factor_c101;
    out.c100={{hit->c100,hit->c100,hit->c100}};
    out.c101=hit->c101;
    out.ptde_specular_power=hit->power;
    out.ptde_specular_power_verified=true;
    out.envspec=mr::ptde_envspec_presence::present;
    return true;
}
inline bool source(const mr::material_identity&m) noexcept{
    mr::decision host{},out{};
    return promote(m,host,33u,out);
}
inline bool draw(const mr::material_identity&m,
                 const mr::decision&decision) noexcept{
    if(!decision.active||decision.receiver_id<33u||
       decision.receiver_id>35u)return false;
    mr::decision host{},proof{};
    if(!promote(m,host,decision.receiver_id,proof))return false;
    if(decision.route_index!=proof.route_index ||
       decision.certified_operations!=proof.certified_operations ||
       decision.receiver_id!=proof.receiver_id ||
       decision.envspec!=proof.envspec ||
       !decision.ptde_specular_power_verified ||
       decision.c101!=proof.c101 ||
       decision.ptde_specular_power!=proof.ptde_specular_power)
        return false;
    for(unsigned i=0;i<3u;++i)
        if(decision.c100[i]!=proof.c100[i])return false;
    return true;
}
}
"""
hdr=root/"include/dsrrl/runtime/v313_exact_slot23_metal_wet.hpp"
hdr.write_text(header,"utf-8")
def patch(path,old,new,name):
    p=root/path;s=p.read_text("utf-8")
    assert s.count(old)==1,(name,path,s.count(old))
    s=s.replace(old,new,1)
    p.write_text(s,"utf-8")
macro="DSRRL_V313_EXACT_METAL2_WET3"
inc='#include "dsrrl/runtime/v313_exact_slot23_metal_wet.hpp"'
gate=f'#if defined({macro})\n{inc}\n#endif'
# The existing V300 physical operator cuts stay authoritative.
for path in ["src/runtime/flver_engine_hooks.cpp",
             "src/runtime/pmetal_env_source_runtime.cpp",
             "src/runtime/pmetal_envspec_draw_runtime.cpp",
             "integrated/integrated_addon.cpp"]:
    patch(path,
      '#include "dsrrl/runtime/v300_exact_leather_material.hpp"',
      '#include "dsrrl/runtime/v300_exact_leather_material.hpp"\n'+gate,
      "header exact slot23")
# Selector/LightBank producer (only exact known material and full original PTDE source).
source="src/runtime/pmetal_env_source_runtime.cpp"
patch(source,
'''    if (v300::source(material))
        return true;
#endif''',
'''    if (v300::source(material))
        return true;
#endif
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
    if (v313::source(material))
        return true;
#endif''',"PTDE source cut")
sel="src/runtime/flver_engine_hooks.cpp"
patch(sel,
'''        || v300::source(identity)
#endif''',
'''        || v300::source(identity)
#endif
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
        || v313::source(identity)
#endif''',"FLVER owner source cut")
draw="src/runtime/pmetal_envspec_draw_runtime.cpp"
patch(draw,
'''    if (v300::draw(material, decision))
        return true;
#endif''',
'''    if (v300::draw(material, decision))
        return true;
#endif
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
    if (v313::draw(material, decision))
        return true;
#endif''',"PTDE draw authorizer")
patch(draw,
'''                    || v300::draw(material, decision)
#endif''',
'''                    || v300::draw(material, decision)
#endif
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
                    || v313::draw(material, decision)
#endif''',"PTDE LERP receiver")
integ="integrated/integrated_addon.cpp"
patch(integ,
'''    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(''',
'''#if defined(DSRRL_V313_EXACT_METAL2_WET3)
    // Never displace an already-selected stock/P_Metal/MR/PTDE island.
    if (!effective_envspec_decision.active) {
        dsrrl::operators::material_response::decision slot23{};
        if (dsrrl::runtime::v313::promote(
                material,decision,receiver_id,slot23))
            effective_envspec_decision=slot23;
    }
#endif
    const bool exact_pmetal_envspec =
        dsrrl::runtime::exact_pmetal_envspec_candidate(''',
"integrated material decision cut")
# Retail MTD exact parser cohort must understand the full raw SHA of
# each independent MTD; otherwise the real material pointer is never
# attested. Place new cohort immediately after V300 (slot0 36, slot1 17).
patch(sel,
'''#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    + dsrrl::runtime::v300::k_leather_count
#endif
    ;''',
'''#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    + dsrrl::runtime::v300::k_leather_count
#endif
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
    + dsrrl::runtime::v313::k_count
#endif
    ;''',"parser material cohort total")
# In the exact raw-SHA classifier, the original v300 case ends directly
# before the common non-match return. A third array extends the same
# disjoint cache ordinal space.
p=root/sel;s=p.read_text("utf-8")
start=s.index("  const std::size_t leather_base=")
end=s.index("#endif\n   return false;\n }\n\n const auto combined_ordinal=",start)
needle=s[start:end]
assert "dsrrl::runtime::v300::k_leather[i].sha" in needle
assert s.count(needle)==1
add=r"""
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
  const std::size_t slot23_base=
      operators::material_response::generated::
          k_material_route_count_v1+
      operators::material_response::generated::
          k_exact_binding_mr_v1.size()+
      dsrrl::runtime::v297::k_authored_slot0_count+
      dsrrl::runtime::v300::k_leather_count;
  std::size_t slot23_found=dsrrl::runtime::v313::k_count;
  for(std::size_t i=0u;i<dsrrl::runtime::v313::k_count;++i){
      if(!dsrrl::runtime::v297::raw_sha_equal(
          digest,dsrrl::runtime::v313::k_rows[i].sha))
          continue;
      if(slot23_found!=dsrrl::runtime::v313::k_count)return false;
      slot23_found=i;
  }
  if(slot23_found!=dsrrl::runtime::v313::k_count){
      route_ordinal=static_cast<std::uint16_t>(
          slot23_base+slot23_found);
      return true;
  }
#endif
"""
# This insertion must be OUTSIDE the V300 #if; it is placed just after
# the original #endif and before the final stock-return.
marker="#endif\n   return false;\n }\n\n const auto combined_ordinal="
assert s.count(marker)==1
s=s.replace(marker,"#endif\n"+add+"   return false;\n }\n\n const auto combined_ordinal=",1)
p.write_text(s,"utf-8")
# Extend parser identity transport when original slot0 and slot1 are past
# their cohort boundaries.
p=root/sel;s=p.read_text("utf-8")
anchor="if(leather_index>=dsrrl::runtime::v300::k_leather_count)return false;"
replacement=r"""if(leather_index>=dsrrl::runtime::v300::k_leather_count){
#if defined(DSRRL_V313_EXACT_METAL2_WET3)
         const auto slot23_index=
             leather_index-dsrrl::runtime::v300::k_leather_count;
         if(slot23_index>=dsrrl::runtime::v313::k_count)return false;
         const auto &slot23=dsrrl::runtime::v313::k_rows[slot23_index];
         identity.route_index=0u;
         identity.semantic_name_hash=
             operators::material_response::mtd_semantic_hash(slot23.name);
         identity.material_family_hash=
             operators::material_response::mtd_semantic_hash("DifSpcBmp");
         for(std::size_t j=0u;j<32u;++j) {
             const int hi=hex_nibble(slot23.sha[2u*j]);
             const int lo=hex_nibble(slot23.sha[2u*j+1u]);
             if(hi<0||lo<0)return false;
             identity.raw_mtd_sha256[j]=
                 static_cast<std::uint8_t>((hi<<4)|lo);
         }
         return true;
#else
         return false;
#endif
     }"""
assert s.count(anchor)==1,"V313 retail MTD decoder offset drift"
s=s.replace(anchor,replacement,1)
p.write_text(s,"utf-8")
# V311 current-parser, exact DSR model SHA/slot owner recovery MUST also
# recognize the two newly authenticated PTDE material groups. This does
# not enable missing receiver/shader or logical stock t1.
p=root/"include/dsrrl/runtime/v311_live_direct_owner_recovery.hpp"
s=p.read_text("utf-8")
anchor='#include "dsrrl/runtime/v300_exact_leather_material.hpp"'
assert s.count(anchor)==1
s=s.replace(anchor,anchor+'\n#if defined(DSRRL_V313_EXACT_METAL2_WET3)\n#include "dsrrl/runtime/v313_exact_slot23_metal_wet.hpp"\n#endif',1)
anchor='''    if(matched!=1 || exact_env_slot>1u)
        return false;'''
replacement='''#if defined(DSRRL_V313_EXACT_METAL2_WET3)
    for(const auto &row:v313::k_rows) {
        if(source_semantic==mr::mtd_semantic_hash(row.name) &&
           v297::raw_sha_equal(live.raw,row.sha)) {
            ++matched;
            exact_env_slot=static_cast<std::uint8_t>(row.env_slot);
        }
    }
#endif
    if(matched!=1 || exact_env_slot>3u)
        return false;'''
assert s.count(anchor)==1
s=s.replace(anchor,replacement,1)
p.write_text(s,"utf-8")
# Exact V310 reconciliation remains deliberately slot0-only.
(root/"data/census/v313_exact_metal2_wet3_mtd_source.json").write_text(
    json.dumps({"schema":"dsrrl.v313.exact_metal2_wet3", "entries":report,
      "total":len(report),"slot2_count":14,"slot3_count":8,
      "binding":"exact FLVER SHA+slot+MTD current retail parser, original PTDE SPX RX33..35 and full atomic draw",
      "direct_DSR_name_target":749,"ptde_low_only_excluded":544,
      "new_runtime_activation":"OPEN","ptde_pixel":"OPEN"},indent=2)+"\n","utf-8")
print("V313_EXACT_ORIGINAL_METAL2_WET3_22_MTD_SOURCE_TO_OPERATOR_PATCH_PASS")
print("V313_NEW_SLOT2_SLOT3_RUNTIME_AND_PIXEL_OPEN")
