#!/usr/bin/env python3
"""V297 retail MTD parser -> exact model owner join; opt-in only.
Must run after V296 and V297 materializer. Does not modify retail EXE.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text("utf-8")
patches=[
(
'#include "dsrrl/runtime/material_owner_producer.hpp"',
'#include "dsrrl/runtime/material_owner_producer.hpp"\n#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)\n#include "dsrrl/runtime/v297_authored_slot0_material.hpp"\n#endif'
),
(
'''    operators::material_response::generated::
        k_exact_binding_mr_v1.size();

static_assert(
    k_exact_runtime_route_count < 0xffffu,''',
'''    operators::material_response::generated::
        k_exact_binding_mr_v1.size()
#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    + 3u
#endif
    ;

static_assert(
    k_exact_runtime_route_count < 0xffffu,'''
),
(
''' if(extension_match==nullptr)
  return false;

 const auto combined_ordinal=''', 
''' if(extension_match==nullptr){
#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
  // Exact retail parser digest, no guessed MTD / texture / shader alias.
  const std::size_t base=
      operators::material_response::generated::
          k_material_route_count_v1+
      operators::material_response::generated::
          k_exact_binding_mr_v1.size();
  std::size_t found=3u;
  for(std::size_t i=0u;i<3u;++i){
   if(!dsrrl::runtime::v297::raw_sha_equal(
       digest,
       dsrrl::runtime::v297::k_authored_slot0[i].dsr_raw_mtd_sha256))
    continue;
   if(found!=3u)return false;
   found=i;
  }
  if(found!=3u){
   route_ordinal=static_cast<std::uint16_t>(base+found);
   return true;
  }
#endif
   return false;
 }

 const auto combined_ordinal='''
),
(
'''  if(extension_index>=
         operators::material_response::generated::
             k_exact_binding_mr_v1.size())
   return false;

  const auto &route=''', 
'''#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
   const auto ext_count=
       operators::material_response::generated::
           k_exact_binding_mr_v1.size();
   if(extension_index>=ext_count){
    const auto pilot_index=extension_index-ext_count;
    if(pilot_index>=3u)return false;
    const auto &row=
        dsrrl::runtime::v297::k_authored_slot0[pilot_index];
    identity.route_index=0u; // Not an ordinary diffuse MR profile.
    identity.semantic_name_hash=
        operators::material_response::
            mtd_semantic_hash(row.name);
    identity.material_family_hash=
        operators::material_response::
            mtd_semantic_hash("DifSpcBmp");
    for(std::size_t j=0u;j<32u;++j){
     const int hi=hex_nibble(row.dsr_raw_mtd_sha256[j*2u]);
     const int lo=hex_nibble(row.dsr_raw_mtd_sha256[j*2u+1u]);
     if(hi<0||lo<0)return false;
     identity.raw_mtd_sha256[j]=
         static_cast<std::uint8_t>((hi<<4)|lo);
    }
    return true; // Diagnostic operator island is separately owner-gated.
   }
#endif
   if(extension_index>=
          operators::material_response::generated::
              k_exact_binding_mr_v1.size())
    return false;

   const auto &route='''
),
(
'''   const auto identity=
       make_actual_material_identity(
           observation);

#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)''',
'''    auto identity=
        make_actual_material_identity(
            observation);

#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    // Exact material pointer from the RETAIL MTD parser AND independently
    // attested full model SHA/slot/Mtd from the source-complete owner corpus.
    // The parser can never substitute for an absent FLVER owner.
    operators::material_response::material_identity parsed_mtd{};
    if (actual_material != nullptr &&
        lookup_exact_runtime_material(actual_material,parsed_mtd) &&
        parsed_mtd.valid &&
        parsed_mtd.actual_material_exact &&
        parsed_mtd.route_index==0u &&
        identity.owner_tuple_exact &&
        identity.raw_mtd_sha256==parsed_mtd.raw_mtd_sha256 &&
        identity.semantic_name_hash==parsed_mtd.semantic_name_hash) {
        identity.actual_material_exact=true;
        identity.material_family_hash=parsed_mtd.material_family_hash;
    }
#endif

#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)'''
)
]
for a,b in patches:
 if s.count(b)==1:continue
 if s.count(a)!=1:raise RuntimeError(f"V297 parser source mismatch count={s.count(a)} anchor={a[:58]!r}")
 s=s.replace(a,b,1)
assert s.count("V297_AUTHORED_SLOT0_DIAG") >= 5
p.write_text(s,encoding="utf-8")
print("V297_RETAIL_MTD_OWNER_BRIDGE_CONSTRUCTION_PASS")
