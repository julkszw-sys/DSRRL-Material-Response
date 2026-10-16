#!/usr/bin/env python3
"""V300 donor-origin census isolated from protected R43 fallback.

PTDE-homologous row: existing exact PTDE EnvSpec AND EnvDiffuse donors,
as with P_Metal. Exact DSR-only row: original native DSR values preserved
as a structural DSR-only fact; runtime stock-operator fallback is NOT yet proved.
Run after V299 source/consumer materializer. Do not alter latest_hook_source.
"""
from pathlib import Path
root=Path(__file__).resolve().parents[1]
path=root/"src/runtime/pmetal_env_source_runtime.cpp"
s=path.read_text(encoding="utf-8")

replacements=[
(
'#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"',
'#include "dsrrl/runtime/pmetal_env_source_runtime.hpp"\n#if defined(DSRRL_V300_DSR_ONLY_EXACT_DONORS)\n#include "dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp"\n#include "dsrrl/runtime/v299_authored_slot0_operator_gate.hpp"\n#endif'
),
(
'''    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);
    if (row == nullptr) {''',
'''    const auto *row = pmetal_env_source_authority::find_row(*bank,row_id);
#if defined(DSRRL_V300_DSR_ONLY_EXACT_DONORS)
    if (row == nullptr) {
        const auto origin =
            v300::classify_dsr_only(
                bank->signature,
                bank->layout_signature,
                bank->live_count,
                bank->count,
                row_id);
        if (origin.provenance == v300::donor_provenance::dsr_only) {
            // BOTH EnvSpec and EnvDiffuse must remain the original retail
            // DSR operator for an extra row; the exact raw DSR values are
            // catalogued in v300_dsr_only_lightbank_donors.hpp. The native
            // original packer still executes after this probe fails.
            static std::atomic<std::uint32_t> logged{0u};
            std::uint32_t bit=0u;
            for(std::uint32_t i=0u;i<v300::k_dsr_only.size();++i)
                if (&v300::k_dsr_only[i]==origin.dsr_values)
                    bit=1u<<i;
            if(bit!=0u && (logged.fetch_or(bit)&bit)==0u) {
                char line[256]{};
                std::snprintf(line,sizeof(line),
                    "[DSRRL V300 DONOR] stage=dsr_only_exact"
                    " bank=%016llx row=%u"
                    " envspec_m=%u envdiffuse_m=%u"
                    " live_stock_routing=UNVERIFIED",
                    static_cast<unsigned long long>(bank->signature),
                    row_id,
                    static_cast<unsigned>(origin.dsr_values->dsr_envspec[3]),
                    static_cast<unsigned>(origin.dsr_values->dsr_envdiffuse[3]));
                reshade::log::message(reshade::log::level::info,line);
            }
        }
    }
#endif
    if (row == nullptr) {'''
)

]
for pre,new in replacements:
    if s.count(new)==1 and s.count(pre)==0: continue
    if s.count(pre)!=1:raise RuntimeError(f"V300 source identity drift ({s.count(pre)}) {pre[:68]!r}")
    s=s.replace(pre,new,1)
path.write_text(s,encoding="utf-8")
print("V303_DONOR_CENSUS_PROTECTED_R43_FALLBACK_UNCHANGED_PASS")
