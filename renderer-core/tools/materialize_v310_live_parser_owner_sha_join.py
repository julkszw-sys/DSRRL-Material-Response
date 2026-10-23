#!/usr/bin/env python3
"""V310 narrowly joins V298 live parser SHA to original exact slot0 FLVER owner.

Apply AFTER V309 and ALL historical V297/V298 parser source materializers.
Source anchor fail-fast; preserves protected R43 LightBank fallback.
"""
from pathlib import Path
r=Path(__file__).resolve().parents[1]
p=r/"src/runtime/flver_engine_hooks.cpp"
s=p.read_text("utf-8")
header='#include "dsrrl/runtime/v298_parser_mtd_witness.hpp"'
assert s.count(header)==1,"V310 requires original V298 witness source materialized"
s=s.replace(header,header+'\n#if defined(DSRRL_V310_LIVE_PARSER_OWNER_SHA_JOIN)\n#include "dsrrl/runtime/v310_live_parser_owner_sha_join.hpp"\n#endif',1)
anchor='''        identity.material_family_hash=parsed_mtd.material_family_hash;
    }
#endif

#if defined(DSRRL_V298_LIVE_MTD_WITNESS)'''
replacement='''        identity.material_family_hash=parsed_mtd.material_family_hash;
    }
#if defined(DSRRL_V298_LIVE_MTD_WITNESS) && defined(DSRRL_V310_LIVE_PARSER_OWNER_SHA_JOIN)
    // V309 live log: valid exact FLVER+slot+MTD semantic owners had a STALE
    // generic-source MTD digest (8a577e.../217f980c...), but the CURRENT
    // DSR retail parser observed the independently original-verified MTD
    // SHA (7ea7d6.../44e118.../af2f...). Do not weaken owner or SHA.
    if (!identity.actual_material_exact && actual_material != nullptr) {
        dsrrl::runtime::v298::parser_mtd_entry live{};
        if (g_v298_live_parser_mtd.lookup(actual_material,live) &&
            live.material==actual_material &&
            dsrrl::runtime::v310::reconcile_exact_slot0_owner_parser(
                identity,live)) {
            static std::atomic<std::uint32_t> reported{0u};
            const std::uint32_t seen=reported.fetch_add(1u);
            if (seen<5u) {
                char msg[256]{};
                std::snprintf(msg,sizeof(msg),
                    "[DSRRL V310] stage=live_parser_owner_sha_reconciled"
                    " original_FLVER_SHA_SLOT_MTD=1"
                    " live_parser_SHA256=SOURCE_EXACT"
                    " material_slot=%u parser_bytes=%u"
                    " permission=slot0_only downstream=STILL_GATED",
                    identity.material_slot,live.raw_bytes);
                reshade::log::message(reshade::log::level::info,msg);
            }
        }
    }
#endif
#endif

#if defined(DSRRL_V298_LIVE_MTD_WITNESS)'''
assert s.count(anchor)==1,"V310 parser/owner join anchor drift"
s=s.replace(anchor,replacement,1)
p.write_text(s,"utf-8")
print("V310_ORIGINAL_PARSER_SHA_OWNER_SLOT0_RECONCILIATION_SOURCE_PASS")
