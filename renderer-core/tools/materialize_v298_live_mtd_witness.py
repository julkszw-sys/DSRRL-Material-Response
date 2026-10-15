#!/usr/bin/env python3
"""V298 diagnostic: actual parser bytes, joined to the exact FLVER selector.
Requires V296+V297 materializers. No speculative atomic authorization.
"""
from pathlib import Path
p=Path(__file__).resolve().parents[1]/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text(encoding="utf-8")

patches=[
('#include "dsrrl/runtime/v297_authored_slot0_material.hpp"',
 '#include "dsrrl/runtime/v297_authored_slot0_material.hpp"\n#if defined(DSRRL_V298_LIVE_MTD_WITNESS)\n#include "dsrrl/runtime/v298_parser_mtd_witness.hpp"\n#endif'),
('std::atomic_bool g_runtime_mtd_classified{false};',
 '''std::atomic_bool g_runtime_mtd_classified{false};
#if defined(DSRRL_V298_LIVE_MTD_WITNESS)
dsrrl::runtime::v298::parser_mtd_cache g_v298_live_parser_mtd{};
std::atomic<std::uint32_t> g_v298_reported_materials{0u};
#endif'''),
(' exact_material_cache_erase(material);\n',
 ''' exact_material_cache_erase(material);
#if defined(DSRRL_V298_LIVE_MTD_WITNESS)
 // Same material pointer is reused; invalidate the old witness BEFORE
 // checking source bounds, so a rejected parser payload cannot stay valid.
 g_v298_live_parser_mtd.erase(material);
#endif
'''),
('''   static_cast<std::size_t>(len));

 std::uint16_t route_ordinal=0u;''',
 '''   static_cast<std::size_t>(len));
#if defined(DSRRL_V298_LIVE_MTD_WITNESS)
 // This SHA is computed from the actual bytes observed by the DSR parser,
 // never reconstructed from an offline source manifest/FLVER owner table.
 g_v298_live_parser_mtd.publish(material,digest,len);
#endif

 std::uint16_t route_ordinal=0u;'''),
('''    }
#endif

#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
   // Full FLVER+slot+semantic match''',
 '''    }
#endif

#if defined(DSRRL_V298_LIVE_MTD_WITNESS)
    // Identify the exact original DSR model SHA+slot+MTD owner first, then
    // compare its attributed raw MTD to the actual retail parser observation.
    // Diagnostic only: disagreement NEVER promotes a receiver or material.
    if (identity.valid && identity.owner_tuple_exact &&
        actual_material != nullptr) {
        const auto &rows=dsrrl::runtime::v297::k_authored_slot0;
        for (std::uint32_t i=0u; i<3u; ++i) {
            if (identity.semantic_name_hash !=
                operators::material_response::
                    mtd_semantic_hash(rows[i].name))
                continue;
            const auto bit=1u<<i;
            if ((g_v298_reported_materials.fetch_or(bit) & bit)!=0u)
                break;
            dsrrl::runtime::v298::parser_mtd_entry live{};
            const bool found=g_v298_live_parser_mtd.lookup(
                actual_material,live);
            char digest_text[65]{};
            if (found) {
                for (unsigned j=0u;j<32u;++j)
                    std::snprintf(digest_text+j*2u,3u,"%02x",
                        static_cast<unsigned>(live.raw[j]));
            }
            const bool same=found &&
                live.raw==identity.raw_mtd_sha256;
            char msg[360]{};
            std::snprintf(msg,sizeof(msg),
                "[DSRRL V298 MTD] stage=live_parser_owner_join"
                " mtd=%s live_sha=%s live_bytes=%u"
                " match=%u owner_full_sha=1 slot=%u"
                " parser_attested=%u action=%s",
                rows[i].name,
                found ? digest_text : "UNAVAILABLE",
                found ? live.raw_bytes : 0u,
                same?1u:0u,
                identity.material_slot,
                identity.actual_material_exact?1u:0u,
                same ? "retain_certified_route" : "stock_fail_open");
            reshade::log::message(
                same ? reshade::log::level::info
                     : reshade::log::level::warning,msg);
            break;
        }
    }
#endif

#if defined(DSRRL_V293_PARSER_EXACT_FLVER_ALIAS)
   // Full FLVER+slot+semantic match''')
]
for a,b in patches:
    if s.count(b)==1: continue
    if s.count(a)!=1:
        raise RuntimeError(f"V298 source drift or missing V297 parser bridge: {a[:65]!r} n={s.count(a)}")
    s=s.replace(a,b,1)
p.write_text(s,encoding="utf-8")
print("V298_LIVE_PARSER_MTD_WITNESS_SOURCE_PASS")
