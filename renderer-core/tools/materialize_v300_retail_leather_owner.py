#!/usr/bin/env python3
"""V300 source-exact retail MTD parser cohort extension, two leather payloads."""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text("utf-8")
changes=[
('''#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    + 3u
#endif
    ;''','''#if defined(DSRRL_V297_AUTHORED_SLOT0_DIAG)
    + 3u
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
    + 2u
#endif
    ;'''),
('''   route_ordinal=static_cast<std::uint16_t>(base+found);
   return true;
  }
#endif
   return false;
 }''','''   route_ordinal=static_cast<std::uint16_t>(base+found);
   return true;
  }
#endif
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
  const std::size_t leather_base=
      operators::material_response::generated::k_material_route_count_v1+
      operators::material_response::generated::k_exact_binding_mr_v1.size()+3u;
  for(std::size_t i=0u;i<2u;++i) {
   if(!dsrrl::runtime::v297::raw_sha_equal(
      digest,dsrrl::runtime::v300::k_leather[i].sha))continue;
   route_ordinal=static_cast<std::uint16_t>(leather_base+i);
   return true;
  }
#endif
   return false;
 }'''),
('''    if(pilot_index>=3u)return false;
    const auto &row=''', '''    if(pilot_index>=3u) {
#if defined(DSRRL_V300_LEATHER_SLOT1_ATOMIC)
     const auto leather_index=pilot_index-3u;
     if(leather_index>=2u)return false;
     const auto &leather=dsrrl::runtime::v300::k_leather[leather_index];
     identity.route_index=0u;
     identity.semantic_name_hash=
         operators::material_response::mtd_semantic_hash(leather.name);
     identity.material_family_hash=
         operators::material_response::mtd_semantic_hash("DifSpcBmp");
     for(std::size_t j=0u;j<32u;++j) {
      const int hi=hex_nibble(leather.sha[2u*j]);
      const int lo=hex_nibble(leather.sha[2u*j+1u]);
      if(hi<0||lo<0)return false;
      identity.raw_mtd_sha256[j]=
          static_cast<std::uint8_t>((hi<<4)|lo);
     }
     return true;
#else
     return false;
#endif
    }
    const auto &row='''')
]
for old,new in changes:
    if s.count(old)==1:s=s.replace(old,new,1)
    elif s.count(old)==0 and s.count(new)==1:pass
    else:raise RuntimeError(f"V300 retail parser source drift: {old[:50]}")
p.write_text(s,"utf-8")
print("V300_RETAIL_PARSER_COHORT_PASS source_SHA_exact=2")
