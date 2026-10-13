#!/usr/bin/env python3
"""V253 PTDE EnvSpec RGBM per bank/row, native V252 generated-source overlay."""
from pathlib import Path
import argparse
P=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
INCLUDE_OLD='#include "dsrrl/runtime/pmetal_envspec_draw_runtime.hpp"'
INCLUDE_NEW=INCLUDE_OLD+'\n#include "dsrrl/runtime/generated_v253_ptde_envspec_rgbm.hpp"'
ANCHOR=r'''    std::array<float,3> carrier3{{
        source.b[0],
        source.b[1],
        source.b[2]
    }};'''
REPLACEMENT=r'''    // V253 PTDE EnvSpec RGB*Multiplier, bank/row certified by PTDE donor.
    // Preserves the same stock DSR selector routing and beta, but not
    // unrelated DSR host-authored EnvSpec RGBM. Missing => fail open stock.
    std::array<float,3> ptde_spec_a{};
    std::array<float,3> ptde_spec_b{};
    const bool ptde_spec_exact =
        v253_ptde_envspec_rgbm::lookup(
            source.bank_signature_a,source.row_id_a,ptde_spec_a) &&
        v253_ptde_envspec_rgbm::lookup(
            source.bank_signature_b,source.row_id_b,ptde_spec_b);
    if (!ptde_spec_exact) {
        if (shader != nullptr) shader->Release();
        env_resources_.release(prepared.env_resources);
        material_resources_.release_prepared_draw(
            prepared.material_resources);
        telemetry::hot_count(source_rejects_);
        effect_fail(effect_fail_mask_,k_effect_fail_source);
        return false;
    }

    if (family == pmetal_envspec_receiver_family::hemenvlerp) {
        static std::atomic<std::uint32_t> witnessed{0u};
        if (witnessed.fetch_add(1u,std::memory_order_relaxed)<12u) {
            char msg[576]{};
            std::snprintf(msg,sizeof(msg),
                "[DSRRL V253 PTDE ENVSPEC RGBM] rx=%u route=%u "
                "bank_a=%016llx row_a=%u bank_b=%016llx row_b=%u "
                "host_a=%.7g,%.7g,%.7g ptde_a=%.7g,%.7g,%.7g "
                "host_b=%.7g,%.7g,%.7g ptde_b=%.7g,%.7g,%.7g "
                "beta=%.7g source=PTDE_LIGHTBANK_EXACT pixel=OPEN",
                static_cast<unsigned>(decision.receiver_id),
                static_cast<unsigned>(decision.route_index),
                static_cast<unsigned long long>(source.bank_signature_a),
                static_cast<unsigned>(source.row_id_a),
                static_cast<unsigned long long>(source.bank_signature_b),
                static_cast<unsigned>(source.row_id_b),
                static_cast<double>(source.a[0]),
                static_cast<double>(source.a[1]),
                static_cast<double>(source.a[2]),
                static_cast<double>(ptde_spec_a[0]),
                static_cast<double>(ptde_spec_a[1]),
                static_cast<double>(ptde_spec_a[2]),
                static_cast<double>(source.b[0]),
                static_cast<double>(source.b[1]),
                static_cast<double>(source.b[2]),
                static_cast<double>(ptde_spec_b[0]),
                static_cast<double>(ptde_spec_b[1]),
                static_cast<double>(ptde_spec_b[2]),
                static_cast<double>(source.beta));
            reshade::log::message(reshade::log::level::info,msg);
        }
    }

    std::array<float,3> carrier3{{
        ptde_spec_b[0],
        ptde_spec_b[1],
        ptde_spec_b[2]
    }};'''
PAYLOAD_OLD=r'''        {
            source.a[0],
            source.a[1],
            source.a[2],
            0.0f
        },'''
PAYLOAD_NEW=r'''        {
            ptde_spec_a[0],
            ptde_spec_a[1],
            ptde_spec_a[2],
            0.0f
        },'''

def edit(text,old,new,label):
    if text.count(new)==1:return text
    if text.count(old)!=1:raise RuntimeError(f"V253 preimage {label} mismatch {text.count(old)}")
    return text.replace(old,new,1)
def main():
    a=argparse.ArgumentParser();a.add_argument("--apply",action="store_true");v=a.parse_args()
    raw=P.read_text(encoding="utf8").replace("\r\n","\n")
    s=edit(raw,INCLUDE_OLD,INCLUDE_NEW,"include")
    s=edit(s,ANCHOR,REPLACEMENT,"ptde_consumer")
    s=edit(s,PAYLOAD_OLD,PAYLOAD_NEW,"ptde_b12")
    if not v.apply and raw!=s:raise RuntimeError("V253 has not been applied")
    if v.apply and raw!=s:P.write_text(s,encoding="utf8",newline="\n")
    print("V253_PTDE_ENVSPEC_RGBM_CONSUMER_"+("APPLIED" if v.apply else "PASS"))
if __name__=="__main__":main()
