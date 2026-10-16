#!/usr/bin/env python3
"""V304: materialize three typed DSR-only donors in active source lookup.

The authoritative CSV already contains 1249 records. This patch wires
the three exact DSR-only records to both EnvSpec and EnvDiffuse source reads;
a separate early draw-consumer gate preserves the *entire* native DSR
operator. Do not change R43 latest() or latest_hook_source().
Run AFTER V303's V300 provenance/census materializer.
"""
from pathlib import Path

root=Path(__file__).resolve().parents[1]
source=root/"src/runtime/pmetal_env_source_runtime.cpp"
draw=root/"src/runtime/pmetal_envspec_draw_runtime.cpp"

def apply(s, pre, new, label):
    if s.count(pre)!=1:
        if s.count(new)==1 and s.count(pre)==0:
            return s
        raise RuntimeError(f"V304 {label}: source drift; pre={s.count(pre)} new={s.count(new)}")
    return s.replace(pre,new,1)

s=source.read_text(encoding="utf-8")
s=apply(s,
'''        if (row_id >= bank.count)
            return false;

        const auto &row =''',
'''        if (row_id >= bank.count) {
#if defined(DSRRL_V304_TYPED_DSR_ONLY_DONOR)
            // The bin remains an exact 1246-row PTDE donor. Append only
            // structurally identified DSR-only rows as a second typed source.
            // Caller has already resolved a known exact donor-bank signature.
            for (const auto &dsr : v300::k_dsr_only) {
                if (signature != dsr.signature ||
                    row_id != dsr.row_id ||
                    bank.count != dsr.ptde_donor_count ||
                    dsr.live_count != bank.count + 1u)
                    continue;
                const auto value=v300::decode_linear(dsr.dsr_envdiffuse);
                out={value[0],value[1],value[2],0.0f};
                return true;
            }
#endif
            return false;
        }

        const auto &row =''',"typed EnvDiffuse lookup")
s=apply(s,
'''    if (row == nullptr) {
        record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const float scale =''',
'''#if defined(DSRRL_V304_TYPED_DSR_ONLY_DONOR)
    if (row == nullptr) {
        const auto typed=v300::classify_dsr_only(
            bank->signature,
            bank->layout_signature,
            bank->live_count,
            bank->count,
            row_id);
        if (typed.provenance==v300::donor_provenance::dsr_only &&
            typed.dsr_values != nullptr) {
            const auto value=v300::decode_linear(typed.dsr_values->dsr_envspec);
            out={value[0],value[1],value[2],0.0f};
            // Both donor provenance and exact physical bank/row are now
            // known. The draw-consumer chooses native DSR, never PTDE.
            endpoint_cache_publish(source,base,count,index,row_id,out,signature);
            record_hook_decode(hook_decode_ok,version_u32,count_u32,index,row_id,signature);
            return true;
        }
    }
#endif
    if (row == nullptr) {
        record_hook_decode(hook_decode_row_unknown,version_u32,count_u32,index,row_id,signature);
        return false;
    }

    const float scale =''',"typed EnvSpec lookup")
source.write_text(s,encoding="utf-8")

s=draw.read_text(encoding="utf-8")
s=apply(s,
'#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"',
'#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"\n#if defined(DSRRL_V304_TYPED_DSR_ONLY_DONOR)\n#include "dsrrl/runtime/v300_dsr_only_lightbank_donors.hpp"\n#endif',
"draw typed donor include")
s=apply(s,
'''    effect_latch(effect_receiver_source_ready_);

    auto *context =''',
'''#if defined(DSRRL_V304_TYPED_DSR_ONLY_DONOR)
    // The exact material selector and packer produce a typed row64 donor;
    // keep BOTH EnvSpec and EnvDiffuse in the native DSR operator. A single
    // DSR-only endpoint in a two-source blend rejects the whole PTDE island.
    // No D3D11 state has been mutated at this point.
    bool dsr_only_draw=false;
    for (const auto &dsr : v300::k_dsr_only) {
        if ((source.bank_signature_a == dsr.signature &&
             source.row_id_a == dsr.row_id) ||
            (source.bank_signature_b == dsr.signature &&
             source.row_id_b == dsr.row_id)) {
            dsr_only_draw=true;
            break;
        }
    }
    if (dsr_only_draw) {
        telemetry::hot_count(source_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_source);
        log_prepare_stage_once(
            1u << 13u,
            "v304_typed_dsr_only_native_draw",
            material,decision,family);
        return false;
    }
#endif
    effect_latch(effect_receiver_source_ready_);

    auto *context =''',"consumer stock-operator gate")
draw.write_text(s,encoding="utf-8")
print("V304_TYPED_DONOR_ACTIVE_LOOKUP_AND_ATOMIC_STOCK_DRAW_SOURCE_PASS")
